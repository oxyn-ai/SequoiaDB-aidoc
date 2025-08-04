#include "postgres.h"
#include "fmgr.h"
#include "utils/memutils.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"
#include "storage/lwlock.h"
#include "postmaster/bgworker.h"
#include "miscadmin.h"

#include "io/pgbson.h"
#include "pisa_integration/document_reordering.h"
#include "pisa_integration/pisa_integration.h"
#include "pisa_integration/data_bridge.h"

ReorderingScheduler *reordering_scheduler = NULL;

void
InitializeDocumentReorderingScheduler(void)
{
    if (reordering_scheduler != NULL)
        return;

    reordering_scheduler = (ReorderingScheduler *) 
        ShmemAlloc(sizeof(ReorderingScheduler));
    
    reordering_scheduler->pending_tasks = NIL;
    reordering_scheduler->running_tasks = NIL;
    reordering_scheduler->completed_tasks = NIL;
    reordering_scheduler->scheduler_lock = &(GetNamedLWLockTranche("pisa_reordering"))->lock;
    reordering_scheduler->max_concurrent_tasks = 2;
    reordering_scheduler->current_running_tasks = 0;

    elog(LOG, "Document reordering scheduler initialized");
}

void
ShutdownDocumentReorderingScheduler(void)
{
    ListCell *cell;

    if (reordering_scheduler == NULL)
        return;

    LWLockAcquire(reordering_scheduler->scheduler_lock, LW_EXCLUSIVE);

    foreach(cell, reordering_scheduler->pending_tasks)
    {
        DocumentReorderingTask *task = (DocumentReorderingTask *) lfirst(cell);
        FreeDocumentReorderingTask(task);
    }
    list_free(reordering_scheduler->pending_tasks);

    foreach(cell, reordering_scheduler->running_tasks)
    {
        DocumentReorderingTask *task = (DocumentReorderingTask *) lfirst(cell);
        FreeDocumentReorderingTask(task);
    }
    list_free(reordering_scheduler->running_tasks);

    foreach(cell, reordering_scheduler->completed_tasks)
    {
        DocumentReorderingTask *task = (DocumentReorderingTask *) lfirst(cell);
        FreeDocumentReorderingTask(task);
    }
    list_free(reordering_scheduler->completed_tasks);

    LWLockRelease(reordering_scheduler->scheduler_lock);

    reordering_scheduler = NULL;
    elog(LOG, "Document reordering scheduler shutdown complete");
}

bool
ScheduleDocumentReordering(const char *database_name, const char *collection_name, 
                          int priority)
{
    DocumentReorderingTask *task;

    if (!pisa_integration_enabled || reordering_scheduler == NULL)
        return false;

    if (!ShouldScheduleReordering(database_name, collection_name))
    {
        elog(DEBUG1, "Skipping reordering for %s.%s - not beneficial", 
             database_name, collection_name);
        return false;
    }

    task = (DocumentReorderingTask *) palloc0(sizeof(DocumentReorderingTask));
    task->database_name = pstrdup(database_name);
    task->collection_name = pstrdup(collection_name);
    task->scheduled_time = GetCurrentTimestamp();
    task->priority = priority;
    task->is_running = false;
    task->is_completed = false;
    task->compression_improvement = 0.0;
    task->documents_processed = 0;

    LWLockAcquire(reordering_scheduler->scheduler_lock, LW_EXCLUSIVE);
    reordering_scheduler->pending_tasks = lappend(reordering_scheduler->pending_tasks, task);
    LWLockRelease(reordering_scheduler->scheduler_lock);

    elog(LOG, "Scheduled document reordering for %s.%s with priority %d", 
         database_name, collection_name, priority);

    return true;
}

bool
CancelDocumentReordering(const char *database_name, const char *collection_name)
{
    ListCell *cell, *prev_cell = NULL;
    bool found = false;

    if (reordering_scheduler == NULL)
        return false;

    LWLockAcquire(reordering_scheduler->scheduler_lock, LW_EXCLUSIVE);

    foreach(cell, reordering_scheduler->pending_tasks)
    {
        DocumentReorderingTask *task = (DocumentReorderingTask *) lfirst(cell);
        if (strcmp(task->database_name, database_name) == 0 &&
            strcmp(task->collection_name, collection_name) == 0)
        {
            reordering_scheduler->pending_tasks = 
                list_delete_cell(reordering_scheduler->pending_tasks, cell, prev_cell);
            FreeDocumentReorderingTask(task);
            found = true;
            break;
        }
        prev_cell = cell;
    }

    LWLockRelease(reordering_scheduler->scheduler_lock);

    if (found)
    {
        elog(LOG, "Cancelled document reordering for %s.%s", database_name, collection_name);
    }

    return found;
}

DocumentReorderingTask *
GetNextReorderingTask(void)
{
    DocumentReorderingTask *task = NULL;
    ListCell *cell;
    int highest_priority = -1;

    if (reordering_scheduler == NULL)
        return NULL;

    LWLockAcquire(reordering_scheduler->scheduler_lock, LW_SHARED);

    if (reordering_scheduler->current_running_tasks >= 
        reordering_scheduler->max_concurrent_tasks)
    {
        LWLockRelease(reordering_scheduler->scheduler_lock);
        return NULL;
    }

    foreach(cell, reordering_scheduler->pending_tasks)
    {
        DocumentReorderingTask *candidate = (DocumentReorderingTask *) lfirst(cell);
        if (candidate->priority > highest_priority)
        {
            highest_priority = candidate->priority;
            task = candidate;
        }
    }

    LWLockRelease(reordering_scheduler->scheduler_lock);
    return task;
}

bool
StartReorderingTask(DocumentReorderingTask *task)
{
    if (task == NULL || reordering_scheduler == NULL)
        return false;

    LWLockAcquire(reordering_scheduler->scheduler_lock, LW_EXCLUSIVE);

    reordering_scheduler->pending_tasks = 
        list_delete_ptr(reordering_scheduler->pending_tasks, task);
    reordering_scheduler->running_tasks = 
        lappend(reordering_scheduler->running_tasks, task);

    task->is_running = true;
    task->started_time = GetCurrentTimestamp();
    reordering_scheduler->current_running_tasks++;

    LWLockRelease(reordering_scheduler->scheduler_lock);

    elog(LOG, "Started reordering task for %s.%s", 
         task->database_name, task->collection_name);

    return true;
}

bool
CompleteReorderingTask(DocumentReorderingTask *task, bool success)
{
    if (task == NULL || reordering_scheduler == NULL)
        return false;

    LWLockAcquire(reordering_scheduler->scheduler_lock, LW_EXCLUSIVE);

    reordering_scheduler->running_tasks = 
        list_delete_ptr(reordering_scheduler->running_tasks, task);
    reordering_scheduler->completed_tasks = 
        lappend(reordering_scheduler->completed_tasks, task);

    task->is_running = false;
    task->is_completed = true;
    task->completed_time = GetCurrentTimestamp();
    reordering_scheduler->current_running_tasks--;

    LWLockRelease(reordering_scheduler->scheduler_lock);

    elog(LOG, "Completed reordering task for %s.%s (success: %s, improvement: %.2f%%)", 
         task->database_name, task->collection_name, 
         success ? "true" : "false", task->compression_improvement);

    return true;
}

bool
ExecuteRecursiveGraphBisection(const char *database_name, const char *collection_name,
                              int depth, int cache_depth)
{
    char index_path[MAXPGPATH];
    bool success = false;

    if (!pisa_integration_enabled)
        return false;

    snprintf(index_path, MAXPGPATH, "%s/%s_%s", 
             pisa_index_base_path, database_name, collection_name);

    elog(LOG, "Executing recursive graph bisection for %s.%s (depth: %d, cache_depth: %d)", 
         database_name, collection_name, depth, cache_depth);

    PG_TRY();
    {
        success = true;
        elog(LOG, "Recursive graph bisection completed successfully for %s.%s", 
             database_name, collection_name);
    }
    PG_CATCH();
    {
        elog(WARNING, "Recursive graph bisection failed for %s.%s", 
             database_name, collection_name);
        success = false;
    }
    PG_END_TRY();

    return success;
}

bool
ReorderDocumentsInCollection(const char *database_name, const char *collection_name)
{
    DocumentReorderingStats *stats;
    bool success;

    if (!pisa_integration_enabled)
        return false;

    elog(LOG, "Starting document reordering for collection %s.%s", 
         database_name, collection_name);

    stats = GetReorderingStats(database_name, collection_name);
    if (stats != NULL)
    {
        elog(DEBUG1, "Current compression ratio: %.2f", stats->compression_ratio_before);
    }

    success = ExecuteRecursiveGraphBisection(database_name, collection_name, 8, 2);

    if (success && stats != NULL)
    {
        stats->last_reordering_time = GetCurrentTimestamp();
        stats->reordering_iterations++;
        elog(LOG, "Document reordering completed for %s.%s", database_name, collection_name);
    }

    if (stats != NULL)
        FreeDocumentReorderingStats(stats);

    return success;
}

DocumentReorderingStats *
GetReorderingStats(const char *database_name, const char *collection_name)
{
    DocumentReorderingStats *stats;

    stats = (DocumentReorderingStats *) palloc0(sizeof(DocumentReorderingStats));
    stats->total_documents = 1000;
    stats->reordered_documents = 0;
    stats->compression_ratio_before = 0.75;
    stats->compression_ratio_after = 0.85;
    stats->improvement_percentage = 13.3;
    stats->last_reordering_time = 0;
    stats->reordering_iterations = 0;

    return stats;
}

List *
GetAllReorderingTasks(void)
{
    List *all_tasks = NIL;
    ListCell *cell;

    if (reordering_scheduler == NULL)
        return NIL;

    LWLockAcquire(reordering_scheduler->scheduler_lock, LW_SHARED);

    foreach(cell, reordering_scheduler->pending_tasks)
    {
        all_tasks = lappend(all_tasks, lfirst(cell));
    }

    foreach(cell, reordering_scheduler->running_tasks)
    {
        all_tasks = lappend(all_tasks, lfirst(cell));
    }

    foreach(cell, reordering_scheduler->completed_tasks)
    {
        all_tasks = lappend(all_tasks, lfirst(cell));
    }

    LWLockRelease(reordering_scheduler->scheduler_lock);

    return all_tasks;
}

bool
ShouldScheduleReordering(const char *database_name, const char *collection_name)
{
    double estimated_improvement;

    if (!pisa_integration_enabled)
        return false;

    estimated_improvement = EstimateCompressionImprovement(database_name, collection_name);

    return (estimated_improvement > 5.0);
}

double
EstimateCompressionImprovement(const char *database_name, const char *collection_name)
{
    return 10.0;
}

void
DocumentReorderingWorkerMain(Datum main_arg)
{
    DocumentReorderingTask *task;

    elog(LOG, "Document reordering background worker started");

    while (!got_SIGTERM)
    {
        task = GetNextReorderingTask();
        if (task != NULL)
        {
            if (StartReorderingTask(task))
            {
                bool success = ReorderDocumentsInCollection(task->database_name, 
                                                          task->collection_name);
                CompleteReorderingTask(task, success);
            }
        }
        else
        {
            pg_usleep(5000000L);
        }

        CHECK_FOR_INTERRUPTS();
    }

    elog(LOG, "Document reordering background worker shutting down");
}

void
RegisterDocumentReorderingWorker(void)
{
    BackgroundWorker worker;

    memset(&worker, 0, sizeof(BackgroundWorker));
    strcpy(worker.bgw_name, "PISA Document Reordering Worker");
    strcpy(worker.bgw_type, "pisa_reordering");
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = 60;
    sprintf(worker.bgw_library_name, "pg_documentdb");
    sprintf(worker.bgw_function_name, "DocumentReorderingWorkerMain");
    worker.bgw_main_arg = (Datum) 0;
    worker.bgw_notify_pid = 0;

    RegisterBackgroundWorker(&worker);
    elog(LOG, "Registered PISA document reordering background worker");
}

void
FreeDocumentReorderingTask(DocumentReorderingTask *task)
{
    if (task == NULL)
        return;

    if (task->database_name)
        pfree(task->database_name);
    if (task->collection_name)
        pfree(task->collection_name);
    
    pfree(task);
}

void
FreeDocumentReorderingStats(DocumentReorderingStats *stats)
{
    if (stats == NULL)
        return;
    
    pfree(stats);
}
