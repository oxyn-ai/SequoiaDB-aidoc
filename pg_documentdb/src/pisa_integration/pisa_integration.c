#ifdef DISABLE_PISA
#include "postgres.h"
#include "nodes/pg_list.h"
#include "io/bson_core.h"
#include "pisa_integration/pisa_integration.h"

bool pisa_integration_enabled = false;
char *pisa_index_base_path = NULL;
int pisa_default_compression = 0;

void InitializePisaIntegration(void) {}
void ShutdownPisaIntegration(void) {}
bool CreatePisaIndex(const char *database_name, const char *collection_name, PisaCompressionType compression_type) { return false; }
bool UpdatePisaIndex(const char *database_name, const char *collection_name, const pgbson *document, bool is_delete) { return false; }
bool DropPisaIndex(const char *database_name, const char *collection_name) { return false; }
List *ExecutePisaTextSearch(PisaQueryContext *context) { return NIL; }
bool ShouldUsePisaForQuery(const char *query_json) { return false; }
char *ConvertBsonToPisaFormat(const pgbson *document) { return NULL; }
pgbson *ConvertPisaResultToBson(const char *pisa_result) { return NULL; }
void RegisterPisaConfigurationParameters(void) {}
#else
#else


#include "postgres.h"
#include "fmgr.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "storage/ipc.h"
#include "miscadmin.h"

#include "pisa_integration/pisa_integration.h"
#include "pisa_integration/data_bridge.h"
#include "pisa_integration/index_sync.h"
#include "pisa_integration/query_router.h"

bool pisa_integration_enabled = false;
char *pisa_index_base_path = NULL;
int pisa_default_compression = PISA_COMPRESSION_BLOCK_SIMDBP;

static bool pisa_initialized = false;

void
InitializePisaIntegration(void)
{
    if (pisa_initialized)
        return;

    if (!pisa_integration_enabled)
    {
        elog(LOG, "PISA integration is disabled");
        return;
    }

    elog(LOG, "Initializing PISA integration subsystem");

    InitializePisaIndexSync();

    if (pisa_index_base_path == NULL)
    {
        pisa_index_base_path = pstrdup("/tmp/pisa_indexes");
        elog(WARNING, "PISA index base path not configured, using default: %s", 
             pisa_index_base_path);
    }

    pisa_initialized = true;
    elog(LOG, "PISA integration initialized successfully");
}

void
ShutdownPisaIntegration(void)
{
    if (!pisa_initialized)
        return;

    elog(LOG, "Shutting down PISA integration subsystem");
    
    ShutdownPisaIndexSync();
    
    if (pisa_index_base_path)
    {
        pfree(pisa_index_base_path);
        pisa_index_base_path = NULL;
    }

    pisa_initialized = false;
    elog(LOG, "PISA integration shutdown complete");
}

bool
CreatePisaIndex(const char *database_name, const char *collection_name, 
                PisaCompressionType compression_type)
{
    PisaCollection *collection;
    List *documents;
    char index_path[MAXPGPATH];
    bool success = false;

    if (!pisa_integration_enabled)
    {
        elog(WARNING, "PISA integration is disabled");
        return false;
    }

    elog(LOG, "Creating PISA index for collection %s.%s", database_name, collection_name);

    snprintf(index_path, MAXPGPATH, "%s/%s_%s", 
             pisa_index_base_path, database_name, collection_name);

    PG_TRY();
    {
        documents = ExportCollectionToPisa(database_name, collection_name, PISA_EXPORT_FULL);
        
        if (documents == NIL)
        {
            elog(WARNING, "No documents found in collection %s.%s", database_name, collection_name);
            return false;
        }

        success = WritePisaDocumentList(documents, index_path);
        
        if (success)
        {
            EnableIndexSync(database_name, collection_name);
            elog(LOG, "PISA index created successfully for %s.%s", database_name, collection_name);
        }
        else
        {
            elog(ERROR, "Failed to create PISA index for %s.%s", database_name, collection_name);
        }
    }
    PG_CATCH();
    {
        elog(ERROR, "Exception occurred while creating PISA index for %s.%s", 
             database_name, collection_name);
        success = false;
    }
    PG_END_TRY();

    return success;
}

bool
UpdatePisaIndex(const char *database_name, const char *collection_name,
                const pgbson *document, bool is_delete)
{
    if (!pisa_integration_enabled)
        return false;

    if (!IsIndexSyncEnabled(database_name, collection_name))
        return false;

    PisaDocumentOperation operation = is_delete ? PISA_OP_DELETE : PISA_OP_UPDATE;
    char *doc_id = GeneratePisaDocumentId(document, 0);

    RegisterDocumentChange(database_name, collection_name, operation, doc_id, document);

    return true;
}

bool
DropPisaIndex(const char *database_name, const char *collection_name)
{
    char index_path[MAXPGPATH];
    
    if (!pisa_integration_enabled)
        return false;

    elog(LOG, "Dropping PISA index for collection %s.%s", database_name, collection_name);

    DisableIndexSync(database_name, collection_name);

    snprintf(index_path, MAXPGPATH, "%s/%s_%s", 
             pisa_index_base_path, database_name, collection_name);

    return true;
}

List *
ExecutePisaTextSearch(PisaQueryContext *context)
{
    List *results = NIL;
    
    if (!pisa_integration_enabled || !context->use_pisa)
        return NIL;

    elog(LOG, "Executing PISA text search for query: %s", context->query_text);

    return results;
}

bool
ShouldUsePisaForQuery(const char *query_json)
{
    QueryRoutingDecision *decision;
    bool use_pisa;

    if (!pisa_integration_enabled)
        return false;

    decision = AnalyzeQuery(query_json);
    use_pisa = decision->use_pisa;
    
    FreeQueryRoutingDecision(decision);
    
    return use_pisa;
}

char *
ConvertBsonToPisaFormat(const pgbson *document)
{
    return ExtractTextContentFromBson(document);
}

pgbson *
ConvertPisaResultToBson(const char *pisa_result)
{
    return NULL;
}

void
RegisterPisaConfigurationParameters(void)
{
    DefineCustomBoolVariable("documentdb.pisa_integration_enabled",
                            "Enable PISA text search integration",
                            "When enabled, DocumentDB will use PISA for enhanced text search capabilities",
                            &pisa_integration_enabled,
                            false,
                            PGC_SIGHUP,
                            0,
                            NULL,
                            NULL,
                            NULL);

    DefineCustomStringVariable("documentdb.pisa_index_base_path",
                              "Base directory for PISA indexes",
                              "Directory where PISA indexes will be stored",
                              &pisa_index_base_path,
                              "/tmp/pisa_indexes",
                              PGC_SIGHUP,
                              0,
                              NULL,
                              NULL,
                              NULL);

    DefineCustomIntVariable("documentdb.pisa_default_compression",
                           "Default compression algorithm for PISA indexes",
                           "Compression algorithm: 0=none, 1=block_simdbp, 2=block_interpolative, 3=block_qmxint",
                           &pisa_default_compression,
                           PISA_COMPRESSION_BLOCK_SIMDBP,
                           0,
                           3,
                           PGC_SIGHUP,
                           0,
                           NULL,
                           NULL,
                           NULL);
}
#endif
