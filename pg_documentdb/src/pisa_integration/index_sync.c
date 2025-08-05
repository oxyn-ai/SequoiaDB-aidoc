#include "postgres.h"
#include "fmgr.h"
#include "utils/memutils.h"
#include "utils/hsearch.h"
#include "storage/lwlock.h"
#include "storage/shmem.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "miscadmin.h"
#include "utils/guc.h"
#include "utils/timestamp.h"

#include "pisa_integration/index_sync.h"
#include "pisa_integration/pisa_integration.h"

HTAB *pisa_sync_hash = NULL;
LWLock *pisa_sync_lock = NULL;

static bool index_sync_initialized = false;
static int max_sync_entries = 1000;

void
InitializePisaIndexSync(void)
{
    HASHCTL hash_ctl;
    bool found;

    if (index_sync_initialized)
        return;

    elog(LOG, "Initializing PISA index synchronization subsystem");

    memset(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize = sizeof(char) * (NAMEDATALEN * 2);
    hash_ctl.entrysize = sizeof(PisaIndexSyncEntry);
    hash_ctl.hcxt = TopMemoryContext;

    pisa_sync_hash = hash_create("PISA Index Sync Hash",
                                max_sync_entries,
                                &hash_ctl,
                                HASH_ELEM | HASH_CONTEXT);

    if (pisa_sync_hash == NULL)
    {
        elog(ERROR, "Failed to create PISA sync hash table");
        return;
    }

    pisa_sync_lock = &(GetNamedLWLockTranche("pisa_sync"))->lock;

    index_sync_initialized = true;
    elog(LOG, "PISA index synchronization initialized successfully");
}

void
ShutdownPisaIndexSync(void)
{
    if (!index_sync_initialized)
        return;

    elog(LOG, "Shutting down PISA index synchronization subsystem");

    if (pisa_sync_hash)
    {
        hash_destroy(pisa_sync_hash);
        pisa_sync_hash = NULL;
    }

    pisa_sync_lock = NULL;
    index_sync_initialized = false;

    elog(LOG, "PISA index synchronization shutdown complete");
}

void
RegisterDocumentChange(const char *database_name, const char *collection_name,
                      PisaDocumentOperation operation, const char *document_id,
                      const pgbson *document)
{
    char sync_key[NAMEDATALEN * 2];
    PisaIndexSyncEntry *sync_entry;
    bool found;

    if (!index_sync_initialized)
    {
        elog(WARNING, "PISA index sync not initialized");
        return;
    }

    snprintf(sync_key, sizeof(sync_key), "%s_%s", database_name, collection_name);

    LWLockAcquire(pisa_sync_lock, LW_EXCLUSIVE);

    sync_entry = (PisaIndexSyncEntry *) hash_search(pisa_sync_hash,
                                                   sync_key,
                                                   HASH_ENTER,
                                                   &found);

    if (!found)
    {
        strncpy(sync_entry->database_name, database_name, NAMEDATALEN);
        strncpy(sync_entry->collection_name, collection_name, NAMEDATALEN);
        sync_entry->needs_full_rebuild = false;
        sync_entry->needs_incremental_update = false;
        sync_entry->last_sync_time = GetCurrentTimestamp();
        sync_entry->pending_operations = 0;
    }

    sync_entry->needs_incremental_update = true;
    sync_entry->pending_operations++;

    elog(DEBUG1, "Registered document change for %s.%s (operation: %d, pending: %d)",
         database_name, collection_name, operation, sync_entry->pending_operations);

    LWLockRelease(pisa_sync_lock);
}

void
ProcessPendingIndexUpdates(void)
{
    HASH_SEQ_STATUS seq_status;
    PisaIndexSyncEntry *sync_entry;

    if (!index_sync_initialized)
        return;

    elog(DEBUG1, "Processing pending PISA index updates");

    LWLockAcquire(pisa_sync_lock, LW_SHARED);

    hash_seq_init(&seq_status, pisa_sync_hash);

    while ((sync_entry = (PisaIndexSyncEntry *) hash_seq_search(&seq_status)) != NULL)
    {
        if (sync_entry->needs_full_rebuild)
        {
            elog(LOG, "Scheduling full rebuild for %s.%s",
                 sync_entry->database_name, sync_entry->collection_name);

            CreatePisaIndex(sync_entry->database_name, 
                           sync_entry->collection_name,
                           pisa_default_compression);

            sync_entry->needs_full_rebuild = false;
            sync_entry->needs_incremental_update = false;
            sync_entry->pending_operations = 0;
            sync_entry->last_sync_time = GetCurrentTimestamp();
        }
        else if (sync_entry->needs_incremental_update && sync_entry->pending_operations > 0)
        {
            elog(DEBUG1, "Processing incremental update for %s.%s (%d operations)",
                 sync_entry->database_name, sync_entry->collection_name,
                 sync_entry->pending_operations);

            sync_entry->needs_incremental_update = false;
            sync_entry->pending_operations = 0;
            sync_entry->last_sync_time = GetCurrentTimestamp();
        }
    }

    LWLockRelease(pisa_sync_lock);

    elog(DEBUG1, "Completed processing pending PISA index updates");
}

void
ScheduleIndexRebuild(const char *database_name, const char *collection_name)
{
    char sync_key[NAMEDATALEN * 2];
    PisaIndexSyncEntry *sync_entry;
    bool found;

    if (!index_sync_initialized)
    {
        elog(WARNING, "PISA index sync not initialized");
        return;
    }

    snprintf(sync_key, sizeof(sync_key), "%s_%s", database_name, collection_name);

    LWLockAcquire(pisa_sync_lock, LW_EXCLUSIVE);

    sync_entry = (PisaIndexSyncEntry *) hash_search(pisa_sync_hash,
                                                   sync_key,
                                                   HASH_ENTER,
                                                   &found);

    if (!found)
    {
        strncpy(sync_entry->database_name, database_name, NAMEDATALEN);
        strncpy(sync_entry->collection_name, collection_name, NAMEDATALEN);
        sync_entry->needs_incremental_update = false;
        sync_entry->last_sync_time = GetCurrentTimestamp();
        sync_entry->pending_operations = 0;
    }

    sync_entry->needs_full_rebuild = true;

    elog(LOG, "Scheduled full index rebuild for %s.%s", database_name, collection_name);

    LWLockRelease(pisa_sync_lock);
}

bool
IsIndexSyncEnabled(const char *database_name, const char *collection_name)
{
    char sync_key[NAMEDATALEN * 2];
    PisaIndexSyncEntry *sync_entry;
    bool enabled = false;

    if (!index_sync_initialized)
        return false;

    snprintf(sync_key, sizeof(sync_key), "%s_%s", database_name, collection_name);

    LWLockAcquire(pisa_sync_lock, LW_SHARED);

    sync_entry = (PisaIndexSyncEntry *) hash_search(pisa_sync_hash,
                                                   sync_key,
                                                   HASH_FIND,
                                                   NULL);

    enabled = (sync_entry != NULL);

    LWLockRelease(pisa_sync_lock);

    return enabled;
}

void
EnableIndexSync(const char *database_name, const char *collection_name)
{
    char sync_key[NAMEDATALEN * 2];
    PisaIndexSyncEntry *sync_entry;
    bool found;

    if (!index_sync_initialized)
    {
        elog(WARNING, "PISA index sync not initialized");
        return;
    }

    snprintf(sync_key, sizeof(sync_key), "%s_%s", database_name, collection_name);

    LWLockAcquire(pisa_sync_lock, LW_EXCLUSIVE);

    sync_entry = (PisaIndexSyncEntry *) hash_search(pisa_sync_hash,
                                                   sync_key,
                                                   HASH_ENTER,
                                                   &found);

    if (!found)
    {
        strncpy(sync_entry->database_name, database_name, NAMEDATALEN);
        strncpy(sync_entry->collection_name, collection_name, NAMEDATALEN);
        sync_entry->needs_full_rebuild = false;
        sync_entry->needs_incremental_update = false;
        sync_entry->last_sync_time = GetCurrentTimestamp();
        sync_entry->pending_operations = 0;

        elog(LOG, "Enabled PISA index sync for %s.%s", database_name, collection_name);
    }

    LWLockRelease(pisa_sync_lock);
}

void
DisableIndexSync(const char *database_name, const char *collection_name)
{
    char sync_key[NAMEDATALEN * 2];
    bool found;

    if (!index_sync_initialized)
        return;

    snprintf(sync_key, sizeof(sync_key), "%s_%s", database_name, collection_name);

    LWLockAcquire(pisa_sync_lock, LW_EXCLUSIVE);

    hash_search(pisa_sync_hash, sync_key, HASH_REMOVE, &found);

    if (found)
    {
        elog(LOG, "Disabled PISA index sync for %s.%s", database_name, collection_name);
    }

    LWLockRelease(pisa_sync_lock);
}

void
PisaIndexSyncWorkerMain(Datum main_arg)
{
    elog(LOG, "Starting PISA index sync background worker");

    BackgroundWorkerUnblockSignals();

    while (!got_SIGTERM)
    {
        int rc;

        rc = WaitLatch(MyLatch,
                      WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
                      30000L,
                      PG_WAIT_EXTENSION);

        ResetLatch(MyLatch);

        if (rc & WL_POSTMASTER_DEATH)
            proc_exit(1);

        if (got_SIGTERM)
            break;

        ProcessPendingIndexUpdates();
    }

    elog(LOG, "PISA index sync background worker shutting down");
    proc_exit(0);
}

void
StartPisaIndexSyncWorker(void)
{
    BackgroundWorker worker;

    memset(&worker, 0, sizeof(BackgroundWorker));
    strcpy(worker.bgw_name, "PISA Index Sync Worker");
    strcpy(worker.bgw_type, "pisa_index_sync");
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = 30;
    strcpy(worker.bgw_library_name, "documentdb");
    strcpy(worker.bgw_function_name, "PisaIndexSyncWorkerMain");
    worker.bgw_main_arg = (Datum) 0;
    worker.bgw_notify_pid = 0;

    RegisterBackgroundWorker(&worker);

    elog(LOG, "Registered PISA index sync background worker");
}
