#include "postgres.h"
#include "fmgr.h"
#include "utils/jsonb.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/timestamp.h"
#include "storage/lwlock.h"
#include "storage/shmem.h"
#include "access/hash.h"
#include "miscadmin.h"
#include "postmaster/bgworker.h"

#include "pisa_integration/query_cache.h"
#include "pisa_integration/pisa_integration.h"

static HTAB *query_cache_hash = NULL;
static PisaCacheStats *cache_stats = NULL;
static LWLock *cache_lock = NULL;
static bool cache_enabled = true;
static int cache_max_entries = PISA_CACHE_MAX_ENTRIES;
static int cache_default_ttl = PISA_CACHE_DEFAULT_TTL;

void
InitializePisaQueryCache(void)
{
    HASHCTL hash_ctl;
    
    if (query_cache_hash != NULL)
        return;
    
    MemSet(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize = PISA_CACHE_KEY_SIZE;
    hash_ctl.entrysize = sizeof(PisaCacheEntry);
    hash_ctl.hcxt = TopMemoryContext;
    
    query_cache_hash = hash_create("PISA Query Cache",
                                  cache_max_entries,
                                  &hash_ctl,
                                  HASH_ELEM | HASH_CONTEXT);
    
    cache_stats = (PisaCacheStats *) MemoryContextAllocZero(TopMemoryContext, 
                                                           sizeof(PisaCacheStats));
    cache_stats->last_reset = GetCurrentTimestamp();
    
    cache_lock = &(GetNamedLWLockTranche("pisa_query_cache"))->lock;
    
    elog(LOG, "PISA query cache initialized with %d max entries", cache_max_entries);
}

void
ShutdownPisaQueryCache(void)
{
    if (query_cache_hash != NULL)
    {
        hash_destroy(query_cache_hash);
        query_cache_hash = NULL;
    }
    
    if (cache_stats != NULL)
    {
        pfree(cache_stats);
        cache_stats = NULL;
    }
    
    elog(LOG, "PISA query cache shutdown completed");
}

char *
GenerateCacheKey(const char *database_name, const char *collection_name, 
                Jsonb *query, Jsonb *options)
{
    char *query_str = JsonbToCString(NULL, &query->root, VARSIZE(query));
    char *options_str = options ? JsonbToCString(NULL, &options->root, VARSIZE(options)) : "";
    
    uint32 hash_value = DatumGetUInt32(hash_any((const unsigned char *) query_str, 
                                               strlen(query_str)));
    
    char *cache_key = psprintf("%s.%s.%u.%s", 
                              database_name, 
                              collection_name, 
                              hash_value,
                              options_str);
    
    if (strlen(cache_key) >= PISA_CACHE_KEY_SIZE)
    {
        cache_key[PISA_CACHE_KEY_SIZE - 1] = '\0';
    }
    
    return cache_key;
}

bool
CacheQuery(const char *cache_key, Jsonb *result, int ttl_seconds)
{
    PisaCacheEntry *entry;
    bool found;
    
    if (!cache_enabled || cache_key == NULL || result == NULL)
        return false;
    
    LWLockAcquire(cache_lock, LW_EXCLUSIVE);
    
    if (hash_get_num_entries(query_cache_hash) >= cache_max_entries)
    {
        EvictLRUEntries(cache_max_entries / 10);
    }
    
    entry = (PisaCacheEntry *) hash_search(query_cache_hash, 
                                          cache_key, 
                                          HASH_ENTER, 
                                          &found);
    
    if (entry != NULL)
    {
        strncpy(entry->cache_key, cache_key, PISA_CACHE_KEY_SIZE - 1);
        entry->cache_key[PISA_CACHE_KEY_SIZE - 1] = '\0';
        
        entry->query_result = result;
        entry->created_at = GetCurrentTimestamp();
        entry->last_accessed = entry->created_at;
        entry->access_count = 0;
        entry->ttl_seconds = ttl_seconds > 0 ? ttl_seconds : cache_default_ttl;
        entry->is_valid = true;
        entry->result_size_bytes = VARSIZE(result);
        
        cache_stats->total_entries++;
        cache_stats->total_size_bytes += entry->result_size_bytes;
    }
    
    LWLockRelease(cache_lock);
    
    elog(DEBUG1, "Cached query result for key: %s (TTL: %d seconds)", 
         cache_key, ttl_seconds);
    
    return entry != NULL;
}

Jsonb *
GetCachedQuery(const char *cache_key)
{
    PisaCacheEntry *entry;
    Jsonb *result = NULL;
    
    if (!cache_enabled || cache_key == NULL)
        return NULL;
    
    LWLockAcquire(cache_lock, LW_SHARED);
    
    entry = (PisaCacheEntry *) hash_search(query_cache_hash, 
                                          cache_key, 
                                          HASH_FIND, 
                                          NULL);
    
    if (entry != NULL && IsCacheEntryValid(entry))
    {
        entry->last_accessed = GetCurrentTimestamp();
        entry->access_count++;
        result = entry->query_result;
        
        UpdateCacheStatistics(true);
        
        elog(DEBUG1, "Cache hit for key: %s (access count: %d)", 
             cache_key, entry->access_count);
    }
    else
    {
        UpdateCacheStatistics(false);
        
        if (entry != NULL && !IsCacheEntryValid(entry))
        {
            elog(DEBUG1, "Cache entry expired for key: %s", cache_key);
        }
        else
        {
            elog(DEBUG1, "Cache miss for key: %s", cache_key);
        }
    }
    
    LWLockRelease(cache_lock);
    
    return result;
}

bool
InvalidateCacheEntry(const char *cache_key)
{
    PisaCacheEntry *entry;
    bool found = false;
    
    if (cache_key == NULL)
        return false;
    
    LWLockAcquire(cache_lock, LW_EXCLUSIVE);
    
    entry = (PisaCacheEntry *) hash_search(query_cache_hash, 
                                          cache_key, 
                                          HASH_REMOVE, 
                                          &found);
    
    if (found && entry != NULL)
    {
        cache_stats->total_entries--;
        cache_stats->total_size_bytes -= entry->result_size_bytes;
        cache_stats->cache_evictions++;
    }
    
    LWLockRelease(cache_lock);
    
    elog(DEBUG1, "Invalidated cache entry for key: %s", cache_key);
    
    return found;
}

bool
InvalidateCacheByPattern(const char *pattern)
{
    HASH_SEQ_STATUS status;
    PisaCacheEntry *entry;
    List *keys_to_remove = NIL;
    ListCell *lc;
    int removed_count = 0;
    
    if (pattern == NULL)
        return false;
    
    LWLockAcquire(cache_lock, LW_EXCLUSIVE);
    
    hash_seq_init(&status, query_cache_hash);
    while ((entry = (PisaCacheEntry *) hash_seq_search(&status)) != NULL)
    {
        if (strstr(entry->cache_key, pattern) != NULL)
        {
            keys_to_remove = lappend(keys_to_remove, pstrdup(entry->cache_key));
        }
    }
    
    foreach(lc, keys_to_remove)
    {
        char *key = (char *) lfirst(lc);
        entry = (PisaCacheEntry *) hash_search(query_cache_hash, 
                                              key, 
                                              HASH_REMOVE, 
                                              NULL);
        if (entry != NULL)
        {
            cache_stats->total_entries--;
            cache_stats->total_size_bytes -= entry->result_size_bytes;
            cache_stats->cache_evictions++;
            removed_count++;
        }
        pfree(key);
    }
    
    LWLockRelease(cache_lock);
    
    list_free(keys_to_remove);
    
    elog(LOG, "Invalidated %d cache entries matching pattern: %s", 
         removed_count, pattern);
    
    return removed_count > 0;
}

void
EvictExpiredEntries(void)
{
    HASH_SEQ_STATUS status;
    PisaCacheEntry *entry;
    List *expired_keys = NIL;
    ListCell *lc;
    TimestampTz current_time = GetCurrentTimestamp();
    int evicted_count = 0;
    
    LWLockAcquire(cache_lock, LW_EXCLUSIVE);
    
    hash_seq_init(&status, query_cache_hash);
    while ((entry = (PisaCacheEntry *) hash_seq_search(&status)) != NULL)
    {
        if (!IsCacheEntryValid(entry))
        {
            expired_keys = lappend(expired_keys, pstrdup(entry->cache_key));
        }
    }
    
    foreach(lc, expired_keys)
    {
        char *key = (char *) lfirst(lc);
        entry = (PisaCacheEntry *) hash_search(query_cache_hash, 
                                              key, 
                                              HASH_REMOVE, 
                                              NULL);
        if (entry != NULL)
        {
            cache_stats->total_entries--;
            cache_stats->total_size_bytes -= entry->result_size_bytes;
            cache_stats->cache_evictions++;
            evicted_count++;
        }
        pfree(key);
    }
    
    LWLockRelease(cache_lock);
    
    list_free(expired_keys);
    
    if (evicted_count > 0)
    {
        elog(DEBUG1, "Evicted %d expired cache entries", evicted_count);
    }
}

static int
compare_cache_entries_by_access_time(const void *a, const void *b)
{
    PisaCacheEntry *ea = *(PisaCacheEntry **)a;
    PisaCacheEntry *eb = *(PisaCacheEntry **)b;
    return timestamptz_cmp_internal(ea->last_accessed, eb->last_accessed);
}

void
EvictLRUEntries(int count)
{
    HASH_SEQ_STATUS status;
    PisaCacheEntry *entry;
    List *lru_entries = NIL;
    ListCell *lc;
    int evicted_count = 0;
    
    hash_seq_init(&status, query_cache_hash);
    while ((entry = (PisaCacheEntry *) hash_seq_search(&status)) != NULL)
    {
        lru_entries = lappend(lru_entries, entry);
    }
    
    lru_entries = list_qsort(lru_entries, compare_cache_entries_by_access_time);
    
    int entries_to_evict = Min(count, list_length(lru_entries));
    for (int i = 0; i < entries_to_evict; i++)
    {
        entry = (PisaCacheEntry *) list_nth(lru_entries, i);
        hash_search(query_cache_hash, entry->cache_key, HASH_REMOVE, NULL);
        cache_stats->total_entries--;
        cache_stats->total_size_bytes -= entry->result_size_bytes;
        cache_stats->cache_evictions++;
        evicted_count++;
    }
    
    list_free(lru_entries);
    
    elog(DEBUG1, "Evicted %d LRU cache entries", evicted_count);
}

bool
IsCacheEntryValid(PisaCacheEntry *entry)
{
    TimestampTz current_time = GetCurrentTimestamp();
    TimestampTz expiry_time;
    
    if (entry == NULL || !entry->is_valid)
        return false;
    
    expiry_time = TimestampTzPlusMilliseconds(entry->created_at, 
                                             entry->ttl_seconds * 1000);
    
    return current_time < expiry_time;
}

PisaCacheStats *
GetCacheStatistics(void)
{
    PisaCacheStats *stats_copy;
    
    LWLockAcquire(cache_lock, LW_SHARED);
    
    stats_copy = (PisaCacheStats *) palloc(sizeof(PisaCacheStats));
    memcpy(stats_copy, cache_stats, sizeof(PisaCacheStats));
    
    if (cache_stats->total_queries > 0)
    {
        stats_copy->hit_ratio = (double) cache_stats->cache_hits / cache_stats->total_queries;
    }
    else
    {
        stats_copy->hit_ratio = 0.0;
    }
    
    LWLockRelease(cache_lock);
    
    return stats_copy;
}

void
UpdateCacheStatistics(bool cache_hit)
{
    cache_stats->total_queries++;
    
    if (cache_hit)
        cache_stats->cache_hits++;
    else
        cache_stats->cache_misses++;
}

Datum
documentdb_cache_pisa_query(PG_FUNCTION_ARGS)
{
    text *cache_key_text = PG_GETARG_TEXT_PP(0);
    Jsonb *result = PG_GETARG_JSONB_P(1);
    int32 ttl_seconds = PG_GETARG_INT32(2);
    
    char *cache_key = text_to_cstring(cache_key_text);
    
    bool success = CacheQuery(cache_key, result, ttl_seconds);
    
    PG_RETURN_BOOL(success);
}

Datum
documentdb_get_cached_pisa_query(PG_FUNCTION_ARGS)
{
    text *cache_key_text = PG_GETARG_TEXT_PP(0);
    char *cache_key = text_to_cstring(cache_key_text);
    
    Jsonb *result = GetCachedQuery(cache_key);
    
    if (result != NULL)
        PG_RETURN_JSONB_P(result);
    else
        PG_RETURN_NULL();
}

Datum
documentdb_invalidate_pisa_cache(PG_FUNCTION_ARGS)
{
    text *pattern_text = PG_GETARG_TEXT_PP(0);
    char *pattern = text_to_cstring(pattern_text);
    
    bool success = InvalidateCacheByPattern(pattern);
    
    PG_RETURN_BOOL(success);
}

Datum
documentdb_get_pisa_cache_stats(PG_FUNCTION_ARGS)
{
    PisaCacheStats *stats = GetCacheStatistics();
    JsonbParseState *state = NULL;
    JsonbValue *result_object;
    
    pushJsonbValue(&state, WJB_BEGIN_OBJECT, NULL);
    
    JsonbValue key, value;
    
    key.type = jbvString;
    key.val.string.len = strlen("total_queries");
    key.val.string.val = "total_queries";
    value.type = jbvNumeric;
    value.val.numeric = int64_to_numeric(stats->total_queries);
    pushJsonbValue(&state, WJB_KEY, &key);
    pushJsonbValue(&state, WJB_VALUE, &value);
    
    key.val.string.len = strlen("cache_hits");
    key.val.string.val = "cache_hits";
    value.val.numeric = int64_to_numeric(stats->cache_hits);
    pushJsonbValue(&state, WJB_KEY, &key);
    pushJsonbValue(&state, WJB_VALUE, &value);
    
    key.val.string.len = strlen("hit_ratio");
    key.val.string.val = "hit_ratio";
    value.val.numeric = float8_to_numeric(stats->hit_ratio);
    pushJsonbValue(&state, WJB_KEY, &key);
    pushJsonbValue(&state, WJB_VALUE, &value);
    
    result_object = pushJsonbValue(&state, WJB_END_OBJECT, NULL);
    
    pfree(stats);
    
    PG_RETURN_JSONB_P(JsonbValueToJsonb(result_object));
}

Datum
documentdb_reset_pisa_cache(PG_FUNCTION_ARGS)
{
    LWLockAcquire(cache_lock, LW_EXCLUSIVE);
    
    hash_destroy(query_cache_hash);
    
    HASHCTL hash_ctl;
    MemSet(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize = PISA_CACHE_KEY_SIZE;
    hash_ctl.entrysize = sizeof(PisaCacheEntry);
    hash_ctl.hcxt = TopMemoryContext;
    
    query_cache_hash = hash_create("PISA Query Cache",
                                  cache_max_entries,
                                  &hash_ctl,
                                  HASH_ELEM | HASH_CONTEXT);
    
    memset(cache_stats, 0, sizeof(PisaCacheStats));
    cache_stats->last_reset = GetCurrentTimestamp();
    
    LWLockRelease(cache_lock);
    
    elog(LOG, "PISA query cache reset completed");
    
    PG_RETURN_BOOL(true);
}
