#ifndef PISA_MEMORY_OPTIMIZATION_H
#define PISA_MEMORY_OPTIMIZATION_H

#include "postgres.h"
#include "fmgr.h"
#include "utils/memutils.h"
#include "storage/lwlock.h"

#define PISA_MEMORY_POOL_SIZE (64 * 1024 * 1024)
#define PISA_MEMORY_BLOCK_SIZE 4096
#define PISA_MEMORY_MAX_POOLS 16

typedef struct PisaMemoryBlock
{
    void *data;
    size_t size;
    bool is_free;
    struct PisaMemoryBlock *next;
    struct PisaMemoryBlock *prev;
} PisaMemoryBlock;

typedef struct PisaMemoryPool
{
    int pool_id;
    void *pool_start;
    size_t pool_size;
    size_t used_size;
    size_t free_size;
    PisaMemoryBlock *free_blocks;
    PisaMemoryBlock *used_blocks;
    int allocation_count;
    int deallocation_count;
    TimestampTz created_at;
    bool is_active;
} PisaMemoryPool;

typedef struct PisaMemoryStats
{
    int64 total_allocated_bytes;
    int64 total_freed_bytes;
    int64 current_usage_bytes;
    int64 peak_usage_bytes;
    int active_allocations;
    int total_allocations;
    int pool_count;
    double fragmentation_ratio;
    TimestampTz last_gc_time;
} PisaMemoryStats;

void InitializePisaMemoryOptimization(void);
void ShutdownPisaMemoryOptimization(void);

void *PisaMemoryAlloc(size_t size);
void *PisaMemoryRealloc(void *ptr, size_t new_size);
void PisaMemoryFree(void *ptr);
void *PisaMemoryAllocZero(size_t size);

PisaMemoryPool *CreateMemoryPool(size_t pool_size);
void DestroyMemoryPool(PisaMemoryPool *pool);
void *AllocateFromPool(PisaMemoryPool *pool, size_t size);
bool FreeToPool(PisaMemoryPool *pool, void *ptr);

void DefragmentMemoryPools(void);
void CompactMemoryPool(PisaMemoryPool *pool);
void GarbageCollectMemory(void);

PisaMemoryStats *GetMemoryStatistics(void);
void ResetMemoryStatistics(void);
void UpdateMemoryStatistics(int64 allocated_bytes, int64 freed_bytes);

bool OptimizeMemoryLayout(void);
void PreallocateMemoryBuffers(size_t buffer_size, int buffer_count);
void ReleaseUnusedMemory(void);

PG_FUNCTION_INFO_V1(documentdb_get_pisa_memory_stats);
PG_FUNCTION_INFO_V1(documentdb_optimize_pisa_memory);
PG_FUNCTION_INFO_V1(documentdb_defragment_pisa_memory);
PG_FUNCTION_INFO_V1(documentdb_gc_pisa_memory);

#endif
