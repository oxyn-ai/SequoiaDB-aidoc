/*-------------------------------------------------------------------------
 * Copyright (c) Microsoft Corporation.  All rights reserved.
 *
 * include/pisa_integration/document_reordering.h
 *
 * Document reordering optimization using PISA's recursive graph bisection
 *
 *-------------------------------------------------------------------------
 */

#ifndef DOCUMENT_REORDERING_H
#define DOCUMENT_REORDERING_H

#include "postgres.h"
#include "storage/lwlock.h"
#include "postmaster/bgworker.h"

#include "io/bson_core.h"
#include "pisa_integration/pisa_integration.h"

typedef struct DocumentReorderingTask
{
    char *database_name;
    char *collection_name;
    TimestampTz scheduled_time;
    TimestampTz started_time;
    TimestampTz completed_time;
    int priority;
    bool is_running;
    bool is_completed;
    double compression_improvement;
    int64 documents_processed;
} DocumentReorderingTask;

typedef struct DocumentReorderingStats
{
    int64 total_documents;
    int64 reordered_documents;
    double compression_ratio_before;
    double compression_ratio_after;
    double improvement_percentage;
    TimestampTz last_reordering_time;
    int reordering_iterations;
} DocumentReorderingStats;

typedef struct ReorderingScheduler
{
    List *pending_tasks;
    List *running_tasks;
    List *completed_tasks;
    LWLock *scheduler_lock;
    int max_concurrent_tasks;
    int current_running_tasks;
} ReorderingScheduler;

void InitializeDocumentReorderingScheduler(void);
void ShutdownDocumentReorderingScheduler(void);

bool ScheduleDocumentReordering(const char *database_name, const char *collection_name, 
                               int priority);
bool CancelDocumentReordering(const char *database_name, const char *collection_name);

DocumentReorderingTask *GetNextReorderingTask(void);
bool StartReorderingTask(DocumentReorderingTask *task);
bool CompleteReorderingTask(DocumentReorderingTask *task, bool success);

bool ExecuteRecursiveGraphBisection(const char *database_name, const char *collection_name,
                                   int depth, int cache_depth);
bool ReorderDocumentsInCollection(const char *database_name, const char *collection_name);

DocumentReorderingStats *GetReorderingStats(const char *database_name, 
                                           const char *collection_name);
List *GetAllReorderingTasks(void);

bool ShouldScheduleReordering(const char *database_name, const char *collection_name);
double EstimateCompressionImprovement(const char *database_name, const char *collection_name);

void DocumentReorderingWorkerMain(Datum main_arg);
void RegisterDocumentReorderingWorker(void);

void FreeDocumentReorderingTask(DocumentReorderingTask *task);
void FreeDocumentReorderingStats(DocumentReorderingStats *stats);

extern ReorderingScheduler *reordering_scheduler;

#endif
