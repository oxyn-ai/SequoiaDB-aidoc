#pragma once

#ifdef DISABLE_PISA
#include "nodes/pg_list.h"
#include "io/bson_core.h"
#include "pisa_integration/pisa_integration.h"

typedef struct PisaQueryPlan PisaQueryPlan;

static inline PisaQueryPlan *PlanPisaQuery(const char *database_name, const char *collection_name, const char *query_json, int limit) { return NULL; }
static inline List *ExecutePisaPlannedQuery(PisaQueryPlan *plan) { return NIL; }
#else

#include "postgres.h"
#include "nodes/parsenodes.h"
#include "utils/jsonb.h"

#include "io/bson_core.h"
#include "pisa_integration/pisa_integration.h"

typedef enum QueryType
{
    QUERY_TYPE_SIMPLE_FIND = 0,
    QUERY_TYPE_TEXT_SEARCH = 1,
    QUERY_TYPE_COMPLEX_AGGREGATION = 2,
    QUERY_TYPE_GEOSPATIAL = 3,
    QUERY_TYPE_VECTOR_SEARCH = 4
} QueryType;

typedef struct QueryRoutingDecision
{
    bool use_pisa;
    bool use_hybrid;
    QueryType query_type;
    char *routing_reason;
    double estimated_cost;
} QueryRoutingDecision;

typedef struct HybridQueryPlan
{
    bool pisa_phase_enabled;
    bool documentdb_phase_enabled;
    char *pisa_query;
    char *documentdb_query;
    int result_merge_strategy;
} HybridQueryPlan;

QueryRoutingDecision *AnalyzeQuery(const char *query_json);
bool ShouldUsePisaForTextSearch(const char *query_json);
bool HasTextSearchCriteria(const pgbson *query_filter);

HybridQueryPlan *CreateHybridQueryPlan(const char *query_json, QueryRoutingDecision *decision);
List *ExecuteHybridQuery(HybridQueryPlan *plan, const char *database_name, const char *collection_name);

List *MergeQueryResults(List *pisa_results, List *documentdb_results, int merge_strategy);
double EstimateQueryCost(const char *query_json, bool use_pisa);

void FreeQueryRoutingDecision(QueryRoutingDecision *decision);
void FreeHybridQueryPlan(HybridQueryPlan *plan);
#endif
