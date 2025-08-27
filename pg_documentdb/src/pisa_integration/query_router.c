#include "postgres.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/jsonb.h"
#include "catalog/pg_type.h"
#include "nodes/parsenodes.h"

#include "io/bson_core.h"
#include "io/bson_traversal.h"
#include "pisa_integration/query_router.h"
#include "pisa_integration/pisa_integration.h"

QueryRoutingDecision *
AnalyzeQuery(const char *query_json)
{
    QueryRoutingDecision *decision;
    pgbson *query_bson;
    bool has_text_search = false;
    bool has_complex_aggregation = false;
    bool has_geospatial = false;
    bool has_vector_search = false;

    decision = (QueryRoutingDecision *) palloc0(sizeof(QueryRoutingDecision));
    
    if (query_json == NULL || strlen(query_json) == 0)
    {
        decision->use_pisa = false;
        decision->use_hybrid = false;
        decision->query_type = QUERY_TYPE_SIMPLE_FIND;
        decision->routing_reason = pstrdup("Empty query");
        decision->estimated_cost = 1.0;
        return decision;
    }

    PG_TRY();
    {
        query_bson = pgbson_from_cstring(query_json, strlen(query_json));
        
        has_text_search = HasTextSearchCriteria(query_bson);
        has_complex_aggregation = HasComplexAggregation(query_bson);
        has_geospatial = HasGeospatialCriteria(query_bson);
        has_vector_search = HasVectorSearchCriteria(query_bson);

        if (has_text_search && !has_geospatial && !has_vector_search)
        {
            decision->use_pisa = true;
            decision->use_hybrid = false;
            decision->query_type = QUERY_TYPE_TEXT_SEARCH;
            decision->routing_reason = pstrdup("Pure text search query");
            decision->estimated_cost = EstimateQueryCost(query_json, true);
        }
        else if (has_text_search && (has_geospatial || has_vector_search))
        {
            decision->use_pisa = false;
            decision->use_hybrid = true;
            decision->query_type = QUERY_TYPE_TEXT_SEARCH;
            decision->routing_reason = pstrdup("Hybrid text + geospatial/vector search");
            decision->estimated_cost = EstimateQueryCost(query_json, false) * 1.2;
        }
        else if (has_complex_aggregation)
        {
            decision->use_pisa = false;
            decision->use_hybrid = false;
            decision->query_type = QUERY_TYPE_COMPLEX_AGGREGATION;
            decision->routing_reason = pstrdup("Complex aggregation pipeline");
            decision->estimated_cost = EstimateQueryCost(query_json, false) * 1.5;
        }
        else if (has_geospatial)
        {
            decision->use_pisa = false;
            decision->use_hybrid = false;
            decision->query_type = QUERY_TYPE_GEOSPATIAL;
            decision->routing_reason = pstrdup("Geospatial query");
            decision->estimated_cost = EstimateQueryCost(query_json, false);
        }
        else if (has_vector_search)
        {
            decision->use_pisa = false;
            decision->use_hybrid = false;
            decision->query_type = QUERY_TYPE_VECTOR_SEARCH;
            decision->routing_reason = pstrdup("Vector similarity search");
            decision->estimated_cost = EstimateQueryCost(query_json, false);
        }
        else
        {
            decision->use_pisa = false;
            decision->use_hybrid = false;
            decision->query_type = QUERY_TYPE_SIMPLE_FIND;
            decision->routing_reason = pstrdup("Simple find query");
            decision->estimated_cost = EstimateQueryCost(query_json, false);
        }
    }
    PG_CATCH();
    {
        decision->use_pisa = false;
        decision->use_hybrid = false;
        decision->query_type = QUERY_TYPE_SIMPLE_FIND;
        decision->routing_reason = pstrdup("Query parsing error");
        decision->estimated_cost = 10.0;
    }
    PG_END_TRY();

    return decision;
}

bool
ShouldUsePisaForTextSearch(const char *query_json)
{
    QueryRoutingDecision *decision;
    bool use_pisa;

    if (!pisa_integration_enabled)
        return false;

    decision = AnalyzeQuery(query_json);
    use_pisa = decision->use_pisa && (decision->query_type == QUERY_TYPE_TEXT_SEARCH);
    
    FreeQueryRoutingDecision(decision);
    
    return use_pisa;
}

bool
HasTextSearchCriteria(const pgbson *query_filter)
{
    bson_iter_t iter;
    bson_t *bson_doc;

    if (query_filter == NULL)
        return false;

    bson_doc = pgbson_get_bson(query_filter);
    if (!bson_iter_init(&iter, bson_doc))
        return false;

    while (bson_iter_next(&iter))
    {
        const char *key = bson_iter_key(&iter);
        bson_type_t type = bson_iter_type(&iter);

        if (strcmp(key, "$text") == 0)
            return true;

        if (strcmp(key, "$search") == 0)
            return true;

        if (type == BSON_TYPE_DOCUMENT)
        {
            bson_iter_t sub_iter;
            if (bson_iter_recurse(&iter, &sub_iter))
            {
                while (bson_iter_next(&sub_iter))
                {
                    const char *sub_key = bson_iter_key(&sub_iter);
                    if (strcmp(sub_key, "$regex") == 0 || 
                        strcmp(sub_key, "$text") == 0 ||
                        strcmp(sub_key, "$search") == 0)
                        return true;
                }
            }
        }
    }

    return false;
}

HybridQueryPlan *
CreateHybridQueryPlan(const char *query_json, QueryRoutingDecision *decision)
{
    HybridQueryPlan *plan;

    if (query_json == NULL || decision == NULL)
        return NULL;

    plan = (HybridQueryPlan *) palloc0(sizeof(HybridQueryPlan));

    if (decision->use_hybrid)
    {
        plan->pisa_phase_enabled = true;
        plan->documentdb_phase_enabled = true;
        plan->pisa_query = ExtractTextSearchQuery(query_json);
        plan->documentdb_query = ExtractNonTextQuery(query_json);
        plan->result_merge_strategy = MERGE_STRATEGY_INTERSECTION;
    }
    else if (decision->use_pisa)
    {
        plan->pisa_phase_enabled = true;
        plan->documentdb_phase_enabled = false;
        plan->pisa_query = pstrdup(query_json);
        plan->documentdb_query = NULL;
        plan->result_merge_strategy = MERGE_STRATEGY_PISA_ONLY;
    }
    else
    {
        plan->pisa_phase_enabled = false;
        plan->documentdb_phase_enabled = true;
        plan->pisa_query = NULL;
        plan->documentdb_query = pstrdup(query_json);
        plan->result_merge_strategy = MERGE_STRATEGY_DOCUMENTDB_ONLY;
    }

    return plan;
}

List *
ExecuteHybridQuery(HybridQueryPlan *plan, const char *database_name, const char *collection_name)
{
    List *pisa_results = NIL;
    List *documentdb_results = NIL;
    List *final_results = NIL;

    if (plan == NULL)
        return NIL;

    PG_TRY();
    {
        if (plan->pisa_phase_enabled && plan->pisa_query)
        {
            PisaQueryContext context;
            context.query_text = plan->pisa_query;
            context.collection_name = (char *) collection_name;
            context.database_name = (char *) database_name;
            context.limit = 1000;
            context.use_pisa = true;

            pisa_results = ExecutePisaTextSearch(&context);
            elog(DEBUG1, "PISA phase returned %d results", list_length(pisa_results));
        }

        if (plan->documentdb_phase_enabled && plan->documentdb_query)
        {
            documentdb_results = ExecuteDocumentDBQuery(database_name, collection_name, plan->documentdb_query);
            elog(DEBUG1, "DocumentDB phase returned %d results", list_length(documentdb_results));
        }

        final_results = MergeQueryResults(pisa_results, documentdb_results, plan->result_merge_strategy);
        elog(DEBUG1, "Hybrid query returned %d final results", list_length(final_results));
    }
    PG_CATCH();
    {
        elog(ERROR, "Failed to execute hybrid query for %s.%s", database_name, collection_name);
    }
    PG_END_TRY();

    return final_results;
}

List *
MergeQueryResults(List *pisa_results, List *documentdb_results, int merge_strategy)
{
    List *merged_results = NIL;
    ListCell *cell;

    switch (merge_strategy)
    {
        case MERGE_STRATEGY_PISA_ONLY:
            merged_results = list_copy(pisa_results);
            break;

        case MERGE_STRATEGY_DOCUMENTDB_ONLY:
            merged_results = list_copy(documentdb_results);
            break;

        case MERGE_STRATEGY_UNION:
            merged_results = list_copy(pisa_results);
            foreach(cell, documentdb_results)
            {
                merged_results = lappend(merged_results, lfirst(cell));
            }
            break;

        case MERGE_STRATEGY_INTERSECTION:
            foreach(cell, pisa_results)
            {
                void *pisa_result = lfirst(cell);
                if (list_member(documentdb_results, pisa_result))
                {
                    merged_results = lappend(merged_results, pisa_result);
                }
            }
            break;

        default:
            merged_results = list_copy(documentdb_results);
            break;
    }

    return merged_results;
}

double
EstimateQueryCost(const char *query_json, bool use_pisa)
{
    double base_cost = 1.0;
    int query_complexity = 1;

    if (query_json == NULL)
        return base_cost;

    query_complexity = strlen(query_json) / 100 + 1;

    if (use_pisa)
    {
        return base_cost * query_complexity * 0.3;
    }
    else
    {
        return base_cost * query_complexity;
    }
}

void
FreeQueryRoutingDecision(QueryRoutingDecision *decision)
{
    if (decision == NULL)
        return;

    if (decision->routing_reason)
        pfree(decision->routing_reason);
    
    pfree(decision);
}

void
FreeHybridQueryPlan(HybridQueryPlan *plan)
{
    if (plan == NULL)
        return;

    if (plan->pisa_query)
        pfree(plan->pisa_query);
    if (plan->documentdb_query)
        pfree(plan->documentdb_query);
    
    pfree(plan);
}

static bool
HasComplexAggregation(const pgbson *query_bson)
{
    bson_iter_t iter;
    bson_t *bson_doc;

    if (query_bson == NULL)
        return false;

    bson_doc = pgbson_get_bson(query_bson);
    if (!bson_iter_init(&iter, bson_doc))
        return false;

    while (bson_iter_next(&iter))
    {
        const char *key = bson_iter_key(&iter);
        
        if (strcmp(key, "pipeline") == 0 || strcmp(key, "aggregate") == 0)
            return true;
    }

    return false;
}

static bool
HasGeospatialCriteria(const pgbson *query_bson)
{
    bson_iter_t iter;
    bson_t *bson_doc;

    if (query_bson == NULL)
        return false;

    bson_doc = pgbson_get_bson(query_bson);
    if (!bson_iter_init(&iter, bson_doc))
        return false;

    while (bson_iter_next(&iter))
    {
        const char *key = bson_iter_key(&iter);
        
        if (strstr(key, "$geo") != NULL || 
            strcmp(key, "$near") == 0 ||
            strcmp(key, "$within") == 0)
            return true;
    }

    return false;
}

static bool
HasVectorSearchCriteria(const pgbson *query_bson)
{
    bson_iter_t iter;
    bson_t *bson_doc;

    if (query_bson == NULL)
        return false;

    bson_doc = pgbson_get_bson(query_bson);
    if (!bson_iter_init(&iter, bson_doc))
        return false;

    while (bson_iter_next(&iter))
    {
        const char *key = bson_iter_key(&iter);
        
        if (strcmp(key, "$vectorSearch") == 0 || 
            strcmp(key, "$knnBeta") == 0)
            return true;
    }

    return false;
}

static char *
ExtractTextSearchQuery(const char *query_json)
{
    return pstrdup(query_json);
}

static char *
ExtractNonTextQuery(const char *query_json)
{
    return pstrdup(query_json);
}

static List *
ExecuteDocumentDBQuery(const char *database_name, const char *collection_name, const char *query)
{
    return NIL;
}
