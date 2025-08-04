#ifndef PISA_SHARDING_SUPPORT_H
#define PISA_SHARDING_SUPPORT_H

#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/jsonb.h"
#include "catalog/pg_type.h"

typedef struct PisaShardConfig
{
    int shard_count;
    int max_concurrent_shards;
    char *shard_base_path;
    bool auto_sharding_enabled;
    double shard_size_threshold_mb;
    int replication_factor;
} PisaShardConfig;

typedef struct PisaShardMapping
{
    Oid collection_id;
    int shard_id;
    char *shard_path;
    int64 document_count;
    int64 size_bytes;
    TimestampTz last_updated;
    bool is_active;
} PisaShardMapping;

typedef struct PisaShardWorker
{
    int worker_id;
    pid_t process_id;
    char *current_task;
    TimestampTz started_at;
    bool is_busy;
    int processed_documents;
} PisaShardWorker;

void InitializePisaSharding(void);
void ShutdownPisaSharding(void);

bool CreateShardMapping(const char *database_name, const char *collection_name, 
                       int shard_count, const char *shard_strategy);
bool DropShardMapping(const char *database_name, const char *collection_name);
bool ReshardCollection(const char *database_name, const char *collection_name, 
                      int new_shard_count);

int GetShardForDocument(const char *database_name, const char *collection_name, 
                       const char *document_id);
List *GetShardsForQuery(const char *database_name, const char *collection_name, 
                       Jsonb *query_filter);

bool ProcessShardInParallel(const char *database_name, const char *collection_name, 
                           int shard_id, const char *operation_type);
bool CoordinateShardedQuery(const char *database_name, const char *collection_name, 
                           Jsonb *query, Jsonb **result);

bool BalanceShards(const char *database_name, const char *collection_name);
bool MigrateDocumentsBetweenShards(const char *database_name, const char *collection_name, 
                                  int source_shard, int target_shard, int document_count);

List *GetShardStatistics(const char *database_name, const char *collection_name);
bool OptimizeShardDistribution(const char *database_name, const char *collection_name);

PG_FUNCTION_INFO_V1(documentdb_create_shard_mapping);
PG_FUNCTION_INFO_V1(documentdb_drop_shard_mapping);
PG_FUNCTION_INFO_V1(documentdb_reshard_collection);
PG_FUNCTION_INFO_V1(documentdb_get_shard_for_document);
PG_FUNCTION_INFO_V1(documentdb_coordinate_sharded_query);
PG_FUNCTION_INFO_V1(documentdb_balance_shards);
PG_FUNCTION_INFO_V1(documentdb_get_shard_statistics);
PG_FUNCTION_INFO_V1(documentdb_optimize_shard_distribution);

#endif
