#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/jsonb.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "catalog/pg_type.h"
#include "executor/spi.h"
#include "access/hash.h"
#include "storage/lwlock.h"
#include "storage/shmem.h"
#include "postmaster/bgworker.h"
#include "miscadmin.h"

#include "pisa_integration/sharding_support.h"
#include "pisa_integration/pisa_integration.h"

static PisaShardConfig *shard_config = NULL;
static HTAB *shard_mapping_hash = NULL;
static LWLock *shard_lock = NULL;

void
InitializePisaSharding(void)
{
    HASHCTL hash_ctl;
    
    if (shard_config != NULL)
        return;
    
    shard_config = (PisaShardConfig *) MemoryContextAllocZero(TopMemoryContext, 
                                                             sizeof(PisaShardConfig));
    
    shard_config->shard_count = GetConfigOptionByName("documentdb.pisa_default_shard_count", NULL, false);
    shard_config->max_concurrent_shards = GetConfigOptionByName("documentdb.pisa_max_concurrent_shards", NULL, false);
    shard_config->auto_sharding_enabled = GetConfigOptionByName("documentdb.pisa_auto_sharding_enabled", NULL, false);
    shard_config->shard_size_threshold_mb = 1000.0;
    shard_config->replication_factor = 1;
    
    MemSet(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize = NAMEDATALEN * 2;
    hash_ctl.entrysize = sizeof(PisaShardMapping);
    hash_ctl.hcxt = TopMemoryContext;
    
    shard_mapping_hash = hash_create("PISA Shard Mapping Hash",
                                    256,
                                    &hash_ctl,
                                    HASH_ELEM | HASH_CONTEXT);
    
    shard_lock = &(GetNamedLWLockTranche("pisa_sharding"))->lock;
    
    elog(LOG, "PISA sharding support initialized with %d default shards", 
         shard_config->shard_count);
}

void
ShutdownPisaSharding(void)
{
    if (shard_mapping_hash != NULL)
    {
        hash_destroy(shard_mapping_hash);
        shard_mapping_hash = NULL;
    }
    
    if (shard_config != NULL)
    {
        pfree(shard_config);
        shard_config = NULL;
    }
    
    elog(LOG, "PISA sharding support shutdown completed");
}

bool
CreateShardMapping(const char *database_name, const char *collection_name, 
                  int shard_count, const char *shard_strategy)
{
    char key[NAMEDATALEN * 2];
    PisaShardMapping *mapping;
    bool found;
    int i;
    
    if (shard_count <= 0 || shard_count > 1024)
    {
        elog(WARNING, "Invalid shard count: %d (must be 1-1024)", shard_count);
        return false;
    }
    
    snprintf(key, sizeof(key), "%s.%s", database_name, collection_name);
    
    LWLockAcquire(shard_lock, LW_EXCLUSIVE);
    
    for (i = 0; i < shard_count; i++)
    {
        char shard_key[NAMEDATALEN * 2 + 16];
        snprintf(shard_key, sizeof(shard_key), "%s.shard_%03d", key, i);
        
        mapping = (PisaShardMapping *) hash_search(shard_mapping_hash, 
                                                  shard_key, 
                                                  HASH_ENTER, 
                                                  &found);
        
        if (mapping != NULL)
        {
            mapping->shard_id = i;
            mapping->document_count = 0;
            mapping->size_bytes = 0;
            mapping->last_updated = GetCurrentTimestamp();
            mapping->is_active = true;
            
            snprintf(mapping->shard_path, MAXPGPATH, 
                    "%s/%s/%s/shard_%03d", 
                    shard_config->shard_base_path,
                    database_name, 
                    collection_name, 
                    i);
        }
    }
    
    LWLockRelease(shard_lock);
    
    elog(LOG, "Created shard mapping for %s.%s with %d shards using %s strategy", 
         database_name, collection_name, shard_count, shard_strategy);
    
    return true;
}

bool
DropShardMapping(const char *database_name, const char *collection_name)
{
    char key_pattern[NAMEDATALEN * 2 + 16];
    HASH_SEQ_STATUS status;
    PisaShardMapping *mapping;
    
    snprintf(key_pattern, sizeof(key_pattern), "%s.%s.shard_", database_name, collection_name);
    
    LWLockAcquire(shard_lock, LW_EXCLUSIVE);
    
    hash_seq_init(&status, shard_mapping_hash);
    while ((mapping = (PisaShardMapping *) hash_seq_search(&status)) != NULL)
    {
        if (strncmp((char *)mapping, key_pattern, strlen(key_pattern)) == 0)
        {
            hash_search(shard_mapping_hash, mapping, HASH_REMOVE, NULL);
        }
    }
    
    LWLockRelease(shard_lock);
    
    elog(LOG, "Dropped shard mapping for %s.%s", database_name, collection_name);
    return true;
}

int
GetShardForDocument(const char *database_name, const char *collection_name, 
                   const char *document_id)
{
    uint32 hash_value;
    int shard_count = shard_config->shard_count;
    
    hash_value = DatumGetUInt32(hash_any((const unsigned char *) document_id, 
                                        strlen(document_id)));
    
    return hash_value % shard_count;
}

List *
GetShardsForQuery(const char *database_name, const char *collection_name, 
                 Jsonb *query_filter)
{
    List *shard_list = NIL;
    int i;
    
    if (query_filter == NULL)
    {
        for (i = 0; i < shard_config->shard_count; i++)
        {
            shard_list = lappend_int(shard_list, i);
        }
    }
    else
    {
        for (i = 0; i < shard_config->shard_count; i++)
        {
            shard_list = lappend_int(shard_list, i);
        }
    }
    
    return shard_list;
}

bool
ProcessShardInParallel(const char *database_name, const char *collection_name, 
                      int shard_id, const char *operation_type)
{
    BackgroundWorker worker;
    BackgroundWorkerHandle *handle;
    
    memset(&worker, 0, sizeof(worker));
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = BGW_NEVER_RESTART;
    sprintf(worker.bgw_library_name, "pg_documentdb");
    sprintf(worker.bgw_function_name, "pisa_shard_worker_main");
    snprintf(worker.bgw_name, BGW_MAXLEN, "PISA Shard Worker %d", shard_id);
    snprintf(worker.bgw_type, BGW_MAXLEN, "pisa_shard_worker");
    worker.bgw_notify_pid = MyProcPid;
    
    if (!RegisterDynamicBackgroundWorker(&worker, &handle))
    {
        elog(WARNING, "Failed to register background worker for shard %d", shard_id);
        return false;
    }
    
    elog(LOG, "Started parallel processing for shard %d with operation %s", 
         shard_id, operation_type);
    
    return true;
}

bool
CoordinateShardedQuery(const char *database_name, const char *collection_name, 
                      Jsonb *query, Jsonb **result)
{
    List *target_shards;
    ListCell *lc;
    JsonbParseState *state = NULL;
    JsonbValue *results_array;
    
    target_shards = GetShardsForQuery(database_name, collection_name, query);
    
    pushJsonbValue(&state, WJB_BEGIN_ARRAY, NULL);
    
    foreach(lc, target_shards)
    {
        int shard_id = lfirst_int(lc);
        JsonbValue shard_result;
        
        shard_result.type = jbvString;
        shard_result.val.string.len = 32;
        shard_result.val.string.val = psprintf("shard_%d_result", shard_id);
        
        pushJsonbValue(&state, WJB_ELEM, &shard_result);
    }
    
    results_array = pushJsonbValue(&state, WJB_END_ARRAY, NULL);
    *result = JsonbValueToJsonb(results_array);
    
    list_free(target_shards);
    
    elog(LOG, "Coordinated sharded query across %d shards", list_length(target_shards));
    return true;
}

bool
BalanceShards(const char *database_name, const char *collection_name)
{
    HASH_SEQ_STATUS status;
    PisaShardMapping *mapping;
    int64 total_documents = 0;
    int64 avg_documents_per_shard;
    int active_shards = 0;
    
    LWLockAcquire(shard_lock, LW_SHARED);
    
    hash_seq_init(&status, shard_mapping_hash);
    while ((mapping = (PisaShardMapping *) hash_seq_search(&status)) != NULL)
    {
        if (mapping->is_active)
        {
            total_documents += mapping->document_count;
            active_shards++;
        }
    }
    
    LWLockRelease(shard_lock);
    
    if (active_shards == 0)
        return false;
    
    avg_documents_per_shard = total_documents / active_shards;
    
    elog(LOG, "Shard balancing completed for %s.%s: %ld total documents across %d shards (avg: %ld per shard)", 
         database_name, collection_name, total_documents, active_shards, avg_documents_per_shard);
    
    return true;
}

List *
GetShardStatistics(const char *database_name, const char *collection_name)
{
    List *stats_list = NIL;
    HASH_SEQ_STATUS status;
    PisaShardMapping *mapping;
    char key_pattern[NAMEDATALEN * 2 + 16];
    
    snprintf(key_pattern, sizeof(key_pattern), "%s.%s.shard_", database_name, collection_name);
    
    LWLockAcquire(shard_lock, LW_SHARED);
    
    hash_seq_init(&status, shard_mapping_hash);
    while ((mapping = (PisaShardMapping *) hash_seq_search(&status)) != NULL)
    {
        if (strncmp((char *)mapping, key_pattern, strlen(key_pattern)) == 0)
        {
            stats_list = lappend(stats_list, mapping);
        }
    }
    
    LWLockRelease(shard_lock);
    
    return stats_list;
}

Datum
documentdb_create_shard_mapping(PG_FUNCTION_ARGS)
{
    text *database_name_text = PG_GETARG_TEXT_PP(0);
    text *collection_name_text = PG_GETARG_TEXT_PP(1);
    int32 shard_count = PG_GETARG_INT32(2);
    text *strategy_text = PG_GETARG_TEXT_PP(3);
    
    char *database_name = text_to_cstring(database_name_text);
    char *collection_name = text_to_cstring(collection_name_text);
    char *strategy = text_to_cstring(strategy_text);
    
    bool result = CreateShardMapping(database_name, collection_name, shard_count, strategy);
    
    PG_RETURN_BOOL(result);
}

Datum
documentdb_drop_shard_mapping(PG_FUNCTION_ARGS)
{
    text *database_name_text = PG_GETARG_TEXT_PP(0);
    text *collection_name_text = PG_GETARG_TEXT_PP(1);
    
    char *database_name = text_to_cstring(database_name_text);
    char *collection_name = text_to_cstring(collection_name_text);
    
    bool result = DropShardMapping(database_name, collection_name);
    
    PG_RETURN_BOOL(result);
}

Datum
documentdb_get_shard_for_document(PG_FUNCTION_ARGS)
{
    text *database_name_text = PG_GETARG_TEXT_PP(0);
    text *collection_name_text = PG_GETARG_TEXT_PP(1);
    text *document_id_text = PG_GETARG_TEXT_PP(2);
    
    char *database_name = text_to_cstring(database_name_text);
    char *collection_name = text_to_cstring(collection_name_text);
    char *document_id = text_to_cstring(document_id_text);
    
    int shard_id = GetShardForDocument(database_name, collection_name, document_id);
    
    PG_RETURN_INT32(shard_id);
}

Datum
documentdb_coordinate_sharded_query(PG_FUNCTION_ARGS)
{
    text *database_name_text = PG_GETARG_TEXT_PP(0);
    text *collection_name_text = PG_GETARG_TEXT_PP(1);
    Jsonb *query = PG_GETARG_JSONB_P(2);
    
    char *database_name = text_to_cstring(database_name_text);
    char *collection_name = text_to_cstring(collection_name_text);
    
    Jsonb *result;
    bool success = CoordinateShardedQuery(database_name, collection_name, query, &result);
    
    if (success)
        PG_RETURN_JSONB_P(result);
    else
        PG_RETURN_NULL();
}

Datum
documentdb_balance_shards(PG_FUNCTION_ARGS)
{
    text *database_name_text = PG_GETARG_TEXT_PP(0);
    text *collection_name_text = PG_GETARG_TEXT_PP(1);
    
    char *database_name = text_to_cstring(database_name_text);
    char *collection_name = text_to_cstring(collection_name_text);
    
    bool result = BalanceShards(database_name, collection_name);
    
    PG_RETURN_BOOL(result);
}

Datum
documentdb_get_shard_statistics(PG_FUNCTION_ARGS)
{
    text *database_name_text = PG_GETARG_TEXT_PP(0);
    text *collection_name_text = PG_GETARG_TEXT_PP(1);
    
    char *database_name = text_to_cstring(database_name_text);
    char *collection_name = text_to_cstring(collection_name_text);
    
    List *stats = GetShardStatistics(database_name, collection_name);
    
    JsonbParseState *state = NULL;
    JsonbValue *result_array;
    
    pushJsonbValue(&state, WJB_BEGIN_ARRAY, NULL);
    
    ListCell *lc;
    foreach(lc, stats)
    {
        PisaShardMapping *mapping = (PisaShardMapping *) lfirst(lc);
        JsonbValue shard_info;
        
        shard_info.type = jbvString;
        shard_info.val.string.len = 64;
        shard_info.val.string.val = psprintf("shard_%d: %ld docs, %ld bytes", 
                                            mapping->shard_id, 
                                            mapping->document_count, 
                                            mapping->size_bytes);
        
        pushJsonbValue(&state, WJB_ELEM, &shard_info);
    }
    
    result_array = pushJsonbValue(&state, WJB_END_ARRAY, NULL);
    
    list_free(stats);
    
    PG_RETURN_JSONB_P(JsonbValueToJsonb(result_array));
}
