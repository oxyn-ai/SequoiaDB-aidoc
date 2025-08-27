/*-------------------------------------------------------------------------
 * Copyright (c) Microsoft Corporation.  All rights reserved.
 *
 * include/opclass/bson_text_pisa.h
 *
 * PISA integration for enhanced text search capabilities
 *
 *-------------------------------------------------------------------------
 */

#ifndef BSON_TEXT_PISA_H
#define BSON_TEXT_PISA_H

#include "postgres.h"
#include "nodes/primnodes.h"
#include "tsearch/ts_type.h"

#include "io/bson_core.h"
#include "opclass/bson_text_gin.h"
#include "pisa_integration/pisa_integration.h"

typedef struct PisaTextIndexData
{
    char *index_path;
    PisaCompressionType compression_type;
    bool is_compressed;
    TimestampTz last_update;
    int document_count;
    int64 index_size_bytes;
} PisaTextIndexData;

typedef struct PisaTextSearchResult
{
    char *document_id;
    double score;
    pgbson *document;
    int64 collection_id;
} PisaTextSearchResult;

typedef struct PisaHybridQueryContext
{
    char *text_query;
    pgbson *filter_criteria;
    bool use_pisa_text;
    bool use_documentdb_vector;
    int limit;
    int offset;
    List *sort_criteria;
} PisaHybridQueryContext;

Expr *GetFuncExprForPisaTextSearch(List *args, bytea *indexOptions,
                                  bool doRuntimeScan,
                                  PisaTextIndexData *pisaIndexData);

List *ExecutePisaTextQuery(const char *database_name, const char *collection_name,
                          const char *query_text, int limit);

List *ExecuteHybridPisaQuery(PisaHybridQueryContext *context);

bool CreatePisaTextIndex(const char *database_name, const char *collection_name,
                        bytea *indexOptions, PisaCompressionType compression_type);

bool UpdatePisaTextIndex(const char *database_name, const char *collection_name,
                        const pgbson *document, bool is_delete);

PisaTextIndexData *GetPisaTextIndexInfo(const char *database_name, 
                                       const char *collection_name);

bool OptimizePisaTextIndex(const char *database_name, const char *collection_name);

double CalculatePisaTextScore(const char *query_text, const pgbson *document);

bool ShouldUsePisaForTextQuery(const char *query_text, int expected_result_count);

List *CombinePisaAndDocumentDBResults(List *pisa_results, List *documentdb_results,
                                     List *sort_criteria, int limit);

void FreePisaTextSearchResult(PisaTextSearchResult *result);
void FreePisaHybridQueryContext(PisaHybridQueryContext *context);

#endif
