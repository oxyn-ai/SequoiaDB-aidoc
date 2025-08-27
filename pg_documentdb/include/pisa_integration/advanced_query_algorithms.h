/*-------------------------------------------------------------------------
 * Copyright (c) Microsoft Corporation.  All rights reserved.
 *
 * include/pisa_integration/advanced_query_algorithms.h
 *
 * Advanced PISA query algorithms integration for DocumentDB
 *
 *-------------------------------------------------------------------------
 */

#ifndef ADVANCED_QUERY_ALGORITHMS_H
#define ADVANCED_QUERY_ALGORITHMS_H

#include "postgres.h"
#include "nodes/primnodes.h"
#include "utils/array.h"

#include "io/bson_core.h"
#include "pisa_integration/pisa_integration.h"

typedef enum PisaQueryAlgorithm
{
    PISA_ALGORITHM_WAND = 1,
    PISA_ALGORITHM_BLOCK_MAX_WAND = 2,
    PISA_ALGORITHM_MAXSCORE = 3,
    PISA_ALGORITHM_RANKED_AND = 4,
    PISA_ALGORITHM_AUTO = 5
} PisaQueryAlgorithm;

typedef struct PisaQueryCursor
{
    char *term;
    uint64_t current_docid;
    double max_score;
    double current_score;
    bool exhausted;
    void *internal_cursor;
} PisaQueryCursor;

typedef struct PisaTopKQueue
{
    int capacity;
    int size;
    double threshold;
    struct {
        uint64_t docid;
        double score;
    } *entries;
} PisaTopKQueue;

typedef struct PisaAdvancedQueryContext
{
    char *database_name;
    char *collection_name;
    List *query_terms;
    PisaQueryAlgorithm algorithm;
    int top_k;
    double score_threshold;
    PisaTopKQueue *result_queue;
    List *cursors;
} PisaAdvancedQueryContext;

typedef struct PisaQueryExecutionPlan
{
    PisaQueryAlgorithm selected_algorithm;
    List *essential_terms;
    List *non_essential_terms;
    double estimated_cost;
    int estimated_results;
    bool use_block_max_optimization;
    bool use_early_termination;
} PisaQueryExecutionPlan;

PisaTopKQueue *CreatePisaTopKQueue(int capacity);
void FreePisaTopKQueue(PisaTopKQueue *queue);
bool PisaTopKQueueWouldEnter(PisaTopKQueue *queue, double score);
bool PisaTopKQueueInsert(PisaTopKQueue *queue, uint64_t docid, double score);
List *PisaTopKQueueGetResults(PisaTopKQueue *queue);

PisaQueryCursor *CreatePisaQueryCursor(const char *term, const char *index_path);
void FreePisaQueryCursor(PisaQueryCursor *cursor);
bool PisaQueryCursorNext(PisaQueryCursor *cursor);
bool PisaQueryCursorNextGeq(PisaQueryCursor *cursor, uint64_t target_docid);
uint64_t PisaQueryCursorDocId(PisaQueryCursor *cursor);
double PisaQueryCursorScore(PisaQueryCursor *cursor);
double PisaQueryCursorMaxScore(PisaQueryCursor *cursor);

PisaQueryExecutionPlan *AnalyzePisaQuery(List *query_terms, int top_k);
PisaQueryAlgorithm SelectOptimalAlgorithm(List *query_terms, int expected_results);

List *ExecutePisaWandQuery(PisaAdvancedQueryContext *context);
List *ExecutePisaBlockMaxWandQuery(PisaAdvancedQueryContext *context);
List *ExecutePisaMaxScoreQuery(PisaAdvancedQueryContext *context);

List *ExecuteAdvancedPisaQuery(const char *database_name, const char *collection_name,
                              List *query_terms, PisaQueryAlgorithm algorithm, int top_k);

bool OptimizeQueryWithEssentialTerms(PisaAdvancedQueryContext *context);
double CalculateQueryUpperBound(List *cursors, int pivot_position);
bool ShouldUseBlockMaxOptimization(List *query_terms, int expected_results);

void FreePisaAdvancedQueryContext(PisaAdvancedQueryContext *context);
void FreePisaQueryExecutionPlan(PisaQueryExecutionPlan *plan);

#endif
