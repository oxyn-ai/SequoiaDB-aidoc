#ifndef PISA_QUERY_CACHE_H
#define PISA_QUERY_CACHE_H

#include "postgres.h"
#include "fmgr.h"
#include "utils/jsonb.h"
#include "utils/hsearch.h"
#include "storage/lwlock.h"

#define PISA_CACHE_MAX_ENTRIES 10000
#define PISA_CACHE_DEFAULT_TTL 300
#define PISA_CACHE_KEY_SIZE 256

typedef struct PisaCacheEntry
{
    char cache_key[PISA_CACHE_KEY_SIZE];
    Jsonb *query_result;
    TimestampTz created_at;
    TimestampTz last_accessed;
    int access_count;
    int ttl_seconds;
    bool is_valid;
    size_t result_size_bytes;
} PisaCacheEntry;

typedef struct PisaCacheStats
{
    int64 total_queries;
    int64 cache_hits;
    int64 cache_misses;
    int64 cache_evictions;
    int64 total_entries;
    int64 total_size_bytes;
    double hit_ratio;
    TimestampTz last_reset;
} PisaCacheStats;

void InitializePisaQueryCache(void);
void ShutdownPisaQueryCache(void);

char *GenerateCacheKey(const char *database_name, const char *collection_name, 
                      Jsonb *query, Jsonb *options);

bool CacheQuery(const char *cache_key, Jsonb *result, int ttl_seconds);
Jsonb *GetCachedQuery(const char *cache_key);
bool InvalidateCacheEntry(const char *cache_key);
bool InvalidateCacheByPattern(const char *pattern);

void EvictExpiredEntries(void);
void EvictLRUEntries(int count);
bool IsCacheEntryValid(PisaCacheEntry *entry);

PisaCacheStats *GetCacheStatistics(void);
void ResetCacheStatistics(void);
void UpdateCacheStatistics(bool cache_hit);

bool SetCacheConfiguration(int max_entries, int default_ttl, bool enabled);
void OptimizeCachePerformance(void);

PG_FUNCTION_INFO_V1(documentdb_cache_pisa_query);
PG_FUNCTION_INFO_V1(documentdb_get_cached_pisa_query);
PG_FUNCTION_INFO_V1(documentdb_invalidate_pisa_cache);
PG_FUNCTION_INFO_V1(documentdb_get_pisa_cache_stats);
PG_FUNCTION_INFO_V1(documentdb_reset_pisa_cache);
PG_FUNCTION_INFO_V1(documentdb_configure_pisa_cache);

#endif
