#include "postgres.h"
#include "fmgr.h"
#include "utils/memutils.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "catalog/pg_type.h"

#include "io/bson_core.h"
#include "opclass/bson_text_pisa.h"
#include "opclass/bson_text_gin.h"
#include "pisa_integration/pisa_integration.h"
#include "pisa_integration/data_bridge.h"
#include "pisa_integration/query_router.h"

Expr *
GetFuncExprForPisaTextSearch(List *args, bytea *indexOptions,
                            bool doRuntimeScan,
                            PisaTextIndexData *pisaIndexData)
{
    Expr *result;
    
    if (!pisa_integration_enabled || pisaIndexData == NULL)
    {
        return GetFuncExprForTextWithIndexOptions(args, indexOptions, 
                                                 doRuntimeScan, NULL);
    }

    result = GetFuncExprForTextWithIndexOptions(args, indexOptions, 
                                               doRuntimeScan, NULL);

    return result;
}

List *
ExecutePisaTextQuery(const char *database_name, const char *collection_name,
                    const char *query_text, int limit)
{
    PisaQueryContext *context;
    List *results = NIL;
    ListCell *cell;

    if (!pisa_integration_enabled)
        return NIL;

    context = (PisaQueryContext *) palloc0(sizeof(PisaQueryContext));
    context->database_name = pstrdup(database_name);
    context->collection_name = pstrdup(collection_name);
    context->query_text = pstrdup(query_text);
    context->limit = limit;
    context->use_pisa = true;

    List *tmp_pisa = NIL;

    PG_TRY();
    {
        tmp_pisa = ExecutePisaTextSearch(context);
        
        foreach(cell, tmp_pisa)
        {
            PisaTextSearchResult *pisa_result = (PisaTextSearchResult *) lfirst(cell);
            results = lappend(results, pisa_result);
        }

        elog(DEBUG1, "PISA text search returned %d results for query: %s", 
             list_length(results), query_text);
    }
    PG_CATCH();
    {
        elog(WARNING, "PISA text search failed, falling back to DocumentDB native search");
        results = NIL;
    }
    PG_END_TRY();

    return results;
}

List *
ExecuteHybridPisaQuery(PisaHybridQueryContext *context)
{
    List *pisa_results = NIL;
    List *documentdb_results = NIL;
    List *combined_results = NIL;

    if (!pisa_integration_enabled || context == NULL)
        return NIL;

    List *tmp_pisa = NIL;
    List *tmp_docdb = NIL;
    List *tmp_combined = NIL;

    PG_TRY();
    {
        if (context->use_pisa_text && context->text_query != NULL)
        {
            tmp_pisa = ExecutePisaTextQuery(context->database_name,
                                            context->collection_name,
                                            context->text_query,
                                            context->limit * 2);
        }

        if (context->use_documentdb_vector || context->filter_criteria != NULL)
        {
            tmp_docdb = NIL;
        }

        tmp_combined = CombinePisaAndDocumentDBResults(tmp_pisa,
                                                       tmp_docdb,
                                                       context->sort_criteria,
                                                       context->limit);

        elog(DEBUG1, "Hybrid query returned %d results (PISA: %d, DocumentDB: %d)",
             list_length(tmp_combined),
             list_length(tmp_pisa),
             list_length(tmp_docdb));
    }
    PG_CATCH();
    {
        elog(WARNING, "Hybrid PISA query failed");
        tmp_combined = NIL;
    }
    PG_END_TRY();

    pisa_results = tmp_pisa;
    documentdb_results = tmp_docdb;
    combined_results = tmp_combined;

    return combined_results;
}

bool
CreatePisaTextIndex(const char *database_name, const char *collection_name,
                   bytea *indexOptions, PisaCompressionType compression_type)
{
    bool success;

    if (!pisa_integration_enabled)
        return false;

    elog(LOG, "Creating PISA text index for %s.%s", database_name, collection_name);

    success = CreatePisaIndex(database_name, collection_name, compression_type);

    if (success)
    {
        elog(LOG, "Successfully created PISA text index for %s.%s", 
             database_name, collection_name);
    }
    else
    {
        elog(ERROR, "Failed to create PISA text index for %s.%s", 
             database_name, collection_name);
    }

    return success;
}

bool
UpdatePisaTextIndex(const char *database_name, const char *collection_name,
                   const pgbson *document, bool is_delete)
{
    if (!pisa_integration_enabled)
        return false;

    return UpdatePisaIndex(database_name, collection_name, document, is_delete);
}

PisaTextIndexData *
GetPisaTextIndexInfo(const char *database_name, const char *collection_name)
{
    PisaTextIndexData *info;
    char index_path[MAXPGPATH];

    if (!pisa_integration_enabled)
        return NULL;

    info = (PisaTextIndexData *) palloc0(sizeof(PisaTextIndexData));
    
    snprintf(index_path, MAXPGPATH, "%s/%s_%s", 
             pisa_index_base_path, database_name, collection_name);
    
    info->index_path = pstrdup(index_path);
    info->compression_type = pisa_default_compression;
    info->is_compressed = true;
    info->last_update = GetCurrentTimestamp();
    info->document_count = 0;
    info->index_size_bytes = 0;

    return info;
}

bool
OptimizePisaTextIndex(const char *database_name, const char *collection_name)
{
    if (!pisa_integration_enabled)
        return false;

    elog(LOG, "Optimizing PISA text index for %s.%s", database_name, collection_name);

    return CreatePisaIndex(database_name, collection_name, pisa_default_compression);
}

double
CalculatePisaTextScore(const char *query_text, const pgbson *document)
{
    char *document_text;
    double score = 0.0;

    if (!pisa_integration_enabled || query_text == NULL || document == NULL)
        return 0.0;

    document_text = ExtractTextContentFromBson(document);
    
    if (document_text != NULL && strlen(document_text) > 0)
    {
        score = 1.0;
    }

    return score;
}

bool
ShouldUsePisaForTextQuery(const char *query_text, int expected_result_count)
{
    if (!pisa_integration_enabled || query_text == NULL)
        return false;

    if (strlen(query_text) < 3)
        return false;

    if (expected_result_count > 10000)
        return true;

    return ShouldUsePisaForQuery(query_text);
}

List *
CombinePisaAndDocumentDBResults(List *pisa_results, List *documentdb_results,
                               List *sort_criteria, int limit)
{
    List *combined = NIL;
    ListCell *cell;
    int count = 0;

    if (pisa_results != NIL)
    {
        foreach(cell, pisa_results)
        {
            if (count >= limit)
                break;
            combined = lappend(combined, lfirst(cell));
            count++;
        }
    }

    if (documentdb_results != NIL && count < limit)
    {
        foreach(cell, documentdb_results)
        {
            if (count >= limit)
                break;
            combined = lappend(combined, lfirst(cell));
            count++;
        }
    }

    return combined;
}

void
FreePisaTextSearchResult(PisaTextSearchResult *result)
{
    if (result == NULL)
        return;

    if (result->document_id)
        pfree(result->document_id);
    if (result->document)
        pfree(result->document);
    
    pfree(result);
}

void
FreePisaHybridQueryContext(PisaHybridQueryContext *context)
{
    if (context == NULL)
        return;

    if (context->text_query)
        pfree(context->text_query);
    if (context->filter_criteria)
        pfree(context->filter_criteria);
    if (context->sort_criteria)
        list_free(context->sort_criteria);
    
    pfree(context);
}
