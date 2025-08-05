#pragma once

#include "postgres.h"
#include "storage/lwlock.h"
#include "utils/hsearch.h"
#include "postmaster/bgworker.h"

#include "io/pgbson.h"
#include "pisa_integration/pisa_integration.h"

typedef struct PisaIndexSyncEntry
{
    char database_name[NAMEDATALEN];
    char collection_name[NAMEDATALEN];
    bool needs_full_rebuild;
    bool needs_incremental_update;
    TimestampTz last_sync_time;
    int pending_operations;
} PisaIndexSyncEntry;

typedef enum PisaDocumentOperation
{
    PISA_OP_INSERT = 0,
    PISA_OP_UPDATE = 1,
    PISA_OP_DELETE = 2
} PisaDocumentOperation;

typedef struct PisaPendingOperation
{
    PisaDocumentOperation operation;
    char *document_id;
    pgbson *document_data;
    TimestampTz timestamp;
    char database_name[NAMEDATALEN];
    char collection_name[NAMEDATALEN];
} PisaPendingOperation;

extern HTAB *pisa_sync_hash;
extern LWLock *pisa_sync_lock;

void InitializePisaIndexSync(void);
void ShutdownPisaIndexSync(void);

void RegisterDocumentChange(const char *database_name, const char *collection_name,
                           PisaDocumentOperation operation, const char *document_id,
                           const pgbson *document);

void ProcessPendingIndexUpdates(void);
void ScheduleIndexRebuild(const char *database_name, const char *collection_name);

bool IsIndexSyncEnabled(const char *database_name, const char *collection_name);
void EnableIndexSync(const char *database_name, const char *collection_name);
void DisableIndexSync(const char *database_name, const char *collection_name);

void PisaIndexSyncWorkerMain(Datum main_arg);
void StartPisaIndexSyncWorker(void);
