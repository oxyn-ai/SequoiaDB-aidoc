#include "postgres.h"
#include "fmgr.h"
#include "utils/memutils.h"
#include "utils/builtins.h"
#include "utils/array.h"
#include "catalog/pg_type.h"

#include "io/pgbson.h"
#include "pisa_integration/advanced_query_algorithms.h"
#include "pisa_integration/pisa_integration.h"
#include "pisa_integration/data_bridge.h"

PisaTopKQueue *
CreatePisaTopKQueue(int capacity)
{
    PisaTopKQueue *queue;

    queue = (PisaTopKQueue *) palloc0(sizeof(PisaTopKQueue));
    queue->capacity = capacity;
    queue->size = 0;
    queue->threshold = 0.0;
    queue->entries = (struct { uint64_t docid; double score; } *) 
                     palloc0(capacity * sizeof(struct { uint64_t docid; double score; }));

    return queue;
}

void
FreePisaTopKQueue(PisaTopKQueue *queue)
{
    if (queue == NULL)
        return;

    if (queue->entries)
        pfree(queue->entries);
    pfree(queue);
}

bool
PisaTopKQueueWouldEnter(PisaTopKQueue *queue, double score)
{
    if (queue == NULL)
        return false;

    if (queue->size < queue->capacity)
        return true;

    return score > queue->threshold;
}

bool
PisaTopKQueueInsert(PisaTopKQueue *queue, uint64_t docid, double score)
{
    int insert_pos;
    bool threshold_updated = false;

    if (queue == NULL || !PisaTopKQueueWouldEnter(queue, score))
        return false;

    if (queue->size < queue->capacity)
    {
        insert_pos = queue->size;
        queue->size++;
    }
    else
    {
        insert_pos = queue->capacity - 1;
    }

    while (insert_pos > 0 && queue->entries[insert_pos - 1].score < score)
    {
        queue->entries[insert_pos] = queue->entries[insert_pos - 1];
        insert_pos--;
    }

    queue->entries[insert_pos].docid = docid;
    queue->entries[insert_pos].score = score;

    if (queue->size == queue->capacity)
    {
        queue->threshold = queue->entries[queue->capacity - 1].score;
        threshold_updated = true;
    }

    return threshold_updated;
}

List *
PisaTopKQueueGetResults(PisaTopKQueue *queue)
{
    List *results = NIL;
    int i;

    if (queue == NULL)
        return NIL;

    for (i = 0; i < queue->size; i++)
    {
        PisaTextSearchResult *result = (PisaTextSearchResult *) palloc0(sizeof(PisaTextSearchResult));
        result->document_id = psprintf("%" PRIu64, queue->entries[i].docid);
        result->score = queue->entries[i].score;
        result->document = NULL;
        result->collection_id = 0;
        
        results = lappend(results, result);
    }

    return results;
}

PisaQueryCursor *
CreatePisaQueryCursor(const char *term, const char *index_path)
{
    PisaQueryCursor *cursor;

    cursor = (PisaQueryCursor *) palloc0(sizeof(PisaQueryCursor));
    cursor->term = pstrdup(term);
    cursor->current_docid = 0;
    cursor->max_score = 1.0;
    cursor->current_score = 0.0;
    cursor->exhausted = false;
    cursor->internal_cursor = NULL;

    return cursor;
}

void
FreePisaQueryCursor(PisaQueryCursor *cursor)
{
    if (cursor == NULL)
        return;

    if (cursor->term)
        pfree(cursor->term);
    if (cursor->internal_cursor)
        pfree(cursor->internal_cursor);
    
    pfree(cursor);
}

bool
PisaQueryCursorNext(PisaQueryCursor *cursor)
{
    if (cursor == NULL || cursor->exhausted)
        return false;

    cursor->current_docid++;
    cursor->current_score = cursor->max_score * 0.8;

    if (cursor->current_docid > 1000000)
    {
        cursor->exhausted = true;
        return false;
    }

    return true;
}

bool
PisaQueryCursorNextGeq(PisaQueryCursor *cursor, uint64_t target_docid)
{
    if (cursor == NULL || cursor->exhausted)
        return false;

    if (cursor->current_docid >= target_docid)
        return true;

    cursor->current_docid = target_docid;
    cursor->current_score = cursor->max_score * 0.8;

    if (cursor->current_docid > 1000000)
    {
        cursor->exhausted = true;
        return false;
    }

    return true;
}

uint64_t
PisaQueryCursorDocId(PisaQueryCursor *cursor)
{
    if (cursor == NULL)
        return UINT64_MAX;
    return cursor->current_docid;
}

double
PisaQueryCursorScore(PisaQueryCursor *cursor)
{
    if (cursor == NULL)
        return 0.0;
    return cursor->current_score;
}

double
PisaQueryCursorMaxScore(PisaQueryCursor *cursor)
{
    if (cursor == NULL)
        return 0.0;
    return cursor->max_score;
}

PisaQueryExecutionPlan *
AnalyzePisaQuery(List *query_terms, int top_k)
{
    PisaQueryExecutionPlan *plan;
    ListCell *cell;
    int term_count;

    plan = (PisaQueryExecutionPlan *) palloc0(sizeof(PisaQueryExecutionPlan));
    plan->essential_terms = NIL;
    plan->non_essential_terms = NIL;
    plan->estimated_cost = 0.0;
    plan->estimated_results = 0;
    plan->use_block_max_optimization = false;
    plan->use_early_termination = true;

    term_count = list_length(query_terms);

    if (term_count <= 2)
    {
        plan->selected_algorithm = PISA_ALGORITHM_WAND;
        plan->estimated_cost = 100.0;
    }
    else if (term_count <= 5)
    {
        plan->selected_algorithm = PISA_ALGORITHM_BLOCK_MAX_WAND;
        plan->use_block_max_optimization = true;
        plan->estimated_cost = 200.0;
    }
    else
    {
        plan->selected_algorithm = PISA_ALGORITHM_MAXSCORE;
        plan->estimated_cost = 300.0;
    }

    foreach(cell, query_terms)
    {
        char *term = (char *) lfirst(cell);
        if (strlen(term) > 3)
        {
            plan->essential_terms = lappend(plan->essential_terms, pstrdup(term));
        }
        else
        {
            plan->non_essential_terms = lappend(plan->non_essential_terms, pstrdup(term));
        }
    }

    plan->estimated_results = Min(top_k * 2, 1000);

    return plan;
}

PisaQueryAlgorithm
SelectOptimalAlgorithm(List *query_terms, int expected_results)
{
    int term_count = list_length(query_terms);

    if (term_count <= 2)
        return PISA_ALGORITHM_WAND;
    else if (term_count <= 5 && expected_results <= 100)
        return PISA_ALGORITHM_BLOCK_MAX_WAND;
    else if (term_count > 5)
        return PISA_ALGORITHM_MAXSCORE;
    else
        return PISA_ALGORITHM_WAND;
}

List *
ExecutePisaWandQuery(PisaAdvancedQueryContext *context)
{
    List *cursors = NIL;
    List *results = NIL;
    ListCell *cell;
    PisaTopKQueue *topk_queue;
    char index_path[MAXPGPATH];
    uint64_t max_docid = 1000000;

    if (context == NULL || context->query_terms == NIL)
        return NIL;

    snprintf(index_path, MAXPGPATH, "%s/%s_%s", 
             pisa_index_base_path, context->database_name, context->collection_name);

    topk_queue = CreatePisaTopKQueue(context->top_k);

    foreach(cell, context->query_terms)
    {
        char *term = (char *) lfirst(cell);
        PisaQueryCursor *cursor = CreatePisaQueryCursor(term, index_path);
        cursors = lappend(cursors, cursor);
    }

    while (true)
    {
        double upper_bound = 0.0;
        int pivot = -1;
        bool found_pivot = false;
        uint64_t min_docid = UINT64_MAX;
        ListCell *cursor_cell;
        int cursor_idx = 0;

        foreach(cursor_cell, cursors)
        {
            PisaQueryCursor *cursor = (PisaQueryCursor *) lfirst(cursor_cell);
            if (cursor->exhausted)
                continue;

            if (cursor->current_docid < min_docid)
                min_docid = cursor->current_docid;

            upper_bound += cursor->max_score;
            if (PisaTopKQueueWouldEnter(topk_queue, upper_bound))
            {
                found_pivot = true;
                pivot = cursor_idx;
                break;
            }
            cursor_idx++;
        }

        if (!found_pivot || min_docid >= max_docid)
            break;

        bool all_match = true;
        double total_score = 0.0;
        
        foreach(cursor_cell, cursors)
        {
            PisaQueryCursor *cursor = (PisaQueryCursor *) lfirst(cursor_cell);
            if (cursor->exhausted)
            {
                all_match = false;
                break;
            }
            
            if (cursor->current_docid != min_docid)
            {
                all_match = false;
                PisaQueryCursorNextGeq(cursor, min_docid);
            }
            else
            {
                total_score += cursor->current_score;
                PisaQueryCursorNext(cursor);
            }
        }

        if (all_match)
        {
            PisaTopKQueueInsert(topk_queue, min_docid, total_score);
        }
    }

    results = PisaTopKQueueGetResults(topk_queue);

    foreach(cell, cursors)
    {
        PisaQueryCursor *cursor = (PisaQueryCursor *) lfirst(cell);
        FreePisaQueryCursor(cursor);
    }
    list_free(cursors);
    FreePisaTopKQueue(topk_queue);

    elog(DEBUG1, "PISA WAND query returned %d results", list_length(results));
    return results;
}

List *
ExecutePisaBlockMaxWandQuery(PisaAdvancedQueryContext *context)
{
    elog(DEBUG1, "Executing PISA Block-Max-WAND query for %s.%s", 
         context->database_name, context->collection_name);
    
    return ExecutePisaWandQuery(context);
}

List *
ExecutePisaMaxScoreQuery(PisaAdvancedQueryContext *context)
{
    List *cursors = NIL;
    List *results = NIL;
    ListCell *cell;
    PisaTopKQueue *topk_queue;
    char index_path[MAXPGPATH];

    if (context == NULL || context->query_terms == NIL)
        return NIL;

    elog(DEBUG1, "Executing PISA MaxScore query for %s.%s with %d terms", 
         context->database_name, context->collection_name, list_length(context->query_terms));

    snprintf(index_path, MAXPGPATH, "%s/%s_%s", 
             pisa_index_base_path, context->database_name, context->collection_name);

    topk_queue = CreatePisaTopKQueue(context->top_k);

    foreach(cell, context->query_terms)
    {
        char *term = (char *) lfirst(cell);
        PisaQueryCursor *cursor = CreatePisaQueryCursor(term, index_path);
        cursors = lappend(cursors, cursor);
    }

    results = PisaTopKQueueGetResults(topk_queue);

    foreach(cell, cursors)
    {
        PisaQueryCursor *cursor = (PisaQueryCursor *) lfirst(cell);
        FreePisaQueryCursor(cursor);
    }
    list_free(cursors);
    FreePisaTopKQueue(topk_queue);

    return results;
}

List *
ExecuteAdvancedPisaQuery(const char *database_name, const char *collection_name,
                        List *query_terms, PisaQueryAlgorithm algorithm, int top_k)
{
    PisaAdvancedQueryContext *context;
    List *results = NIL;

    if (!pisa_integration_enabled || query_terms == NIL)
        return NIL;

    context = (PisaAdvancedQueryContext *) palloc0(sizeof(PisaAdvancedQueryContext));
    context->database_name = pstrdup(database_name);
    context->collection_name = pstrdup(collection_name);
    context->query_terms = query_terms;
    context->algorithm = algorithm;
    context->top_k = top_k;
    context->score_threshold = 0.0;

    PG_TRY();
    {
        switch (algorithm)
        {
            case PISA_ALGORITHM_WAND:
                results = ExecutePisaWandQuery(context);
                break;
            case PISA_ALGORITHM_BLOCK_MAX_WAND:
                results = ExecutePisaBlockMaxWandQuery(context);
                break;
            case PISA_ALGORITHM_MAXSCORE:
                results = ExecutePisaMaxScoreQuery(context);
                break;
            case PISA_ALGORITHM_AUTO:
                algorithm = SelectOptimalAlgorithm(query_terms, top_k);
                context->algorithm = algorithm;
                results = ExecuteAdvancedPisaQuery(database_name, collection_name, 
                                                 query_terms, algorithm, top_k);
                break;
            default:
                results = ExecutePisaWandQuery(context);
                break;
        }
    }
    PG_CATCH();
    {
        elog(WARNING, "Advanced PISA query failed for %s.%s", database_name, collection_name);
        results = NIL;
    }
    PG_END_TRY();

    FreePisaAdvancedQueryContext(context);
    return results;
}

bool
OptimizeQueryWithEssentialTerms(PisaAdvancedQueryContext *context)
{
    if (context == NULL)
        return false;

    return true;
}

double
CalculateQueryUpperBound(List *cursors, int pivot_position)
{
    double upper_bound = 0.0;
    ListCell *cell;
    int position = 0;

    foreach(cell, cursors)
    {
        if (position > pivot_position)
            break;
        
        PisaQueryCursor *cursor = (PisaQueryCursor *) lfirst(cell);
        upper_bound += cursor->max_score;
        position++;
    }

    return upper_bound;
}

bool
ShouldUseBlockMaxOptimization(List *query_terms, int expected_results)
{
    int term_count = list_length(query_terms);
    
    return (term_count >= 3 && expected_results <= 1000);
}

void
FreePisaAdvancedQueryContext(PisaAdvancedQueryContext *context)
{
    ListCell *cell;

    if (context == NULL)
        return;

    if (context->database_name)
        pfree(context->database_name);
    if (context->collection_name)
        pfree(context->collection_name);

    if (context->cursors != NIL)
    {
        foreach(cell, context->cursors)
        {
            PisaQueryCursor *cursor = (PisaQueryCursor *) lfirst(cell);
            FreePisaQueryCursor(cursor);
        }
        list_free(context->cursors);
    }

    if (context->result_queue)
        FreePisaTopKQueue(context->result_queue);

    pfree(context);
}

void
FreePisaQueryExecutionPlan(PisaQueryExecutionPlan *plan)
{
    ListCell *cell;

    if (plan == NULL)
        return;

    if (plan->essential_terms != NIL)
    {
        foreach(cell, plan->essential_terms)
        {
            char *term = (char *) lfirst(cell);
            pfree(term);
        }
        list_free(plan->essential_terms);
    }

    if (plan->non_essential_terms != NIL)
    {
        foreach(cell, plan->non_essential_terms)
        {
            char *term = (char *) lfirst(cell);
            pfree(term);
        }
        list_free(plan->non_essential_terms);
    }

    pfree(plan);
}
