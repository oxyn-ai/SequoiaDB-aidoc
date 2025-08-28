/*-------------------------------------------------------------------------
 * Copyright (c) Microsoft Corporation.  All rights reserved.
 *
 * include/planner/sql_query_router.h
 *
 * SQL Query Router for DocumentDB - enables SQL queries on document collections
 *
 *-------------------------------------------------------------------------
 */

#ifndef SQL_QUERY_ROUTER_H
#define SQL_QUERY_ROUTER_H

#include "postgres.h"
#include "nodes/plannodes.h"
#include "nodes/parsenodes.h"
#include "nodes/params.h"

#include "metadata/collection.h"
#include "io/bson_core.h"

/*
 * Query type enumeration for routing different query types
 */
typedef enum
{
	QUERY_TYPE_MONGODB,           /* MongoDB API query */
	QUERY_TYPE_SQL_ON_DOCUMENTS,  /* SQL query on DocumentDB collections */
	QUERY_TYPE_STANDARD_SQL       /* Standard PostgreSQL query */
} QueryType;

/*
 * SQL query information extracted from parsed query
 */
typedef struct SQLQueryInfo
{
	char *tableName;              /* Target table name */
	List *selectList;             /* SELECT clause items */
	Node *whereClause;            /* WHERE clause conditions */
	List *fromClause;             /* FROM clause items */
	List *groupClause;            /* GROUP BY clause */
	Node *havingClause;           /* HAVING clause */
	List *orderClause;            /* ORDER BY clause */
	Node *limitClause;            /* LIMIT clause */
} SQLQueryInfo;

/*
 * BSON path mapping for SQL column to BSON field conversion
 */
typedef struct BSONPathMapping
{
	char *sqlColumn;              /* SQL column name */
	char *bsonPath;               /* BSON field path (e.g., "user.profile.age") */
	Oid sqlType;                  /* PostgreSQL type OID */
	bool isNullable;              /* Whether field can be null */
} BSONPathMapping;

/*
 * Schema inference result for a collection
 */
typedef struct InferredSchema
{
	uint64 collectionId;          /* Collection identifier */
	int fieldCount;               /* Number of inferred fields */
	BSONPathMapping *mappings;    /* Field mappings */
	Timestamp lastUpdated;        /* Last schema update time */
	int sampleSize;               /* Number of documents sampled */
} InferredSchema;

/* Main query routing functions */
extern QueryType IdentifyQueryType(Query *parse, const char *queryString);
extern PlannedStmt *ProcessSQLOnDocumentsQuery(Query *parse, const char *queryString,
											   int cursorOptions, ParamListInfo boundParams);

/* Query analysis functions */
extern bool IsDocumentDBDataSchemaQuery(Query *parse);
extern bool ContainsMongoDBFunctions(Query *parse);
extern SQLQueryInfo *ParseSQLQuery(Query *parse);

/* Schema inference functions */
extern InferredSchema *InferSchemaFromCollection(MongoCollection *collection);
extern InferredSchema *GetCachedSchema(uint64 collectionId);
extern void UpdateSchemaCache(uint64 collectionId, InferredSchema *schema);
extern void InvalidateSchemaCache(uint64 collectionId);

/* SQL to BSON conversion functions */
extern pgbson *ConvertSQLWhereToBSON(Node *whereClause, InferredSchema *schema);
extern pgbson *ConvertSQLSelectToBSON(List *selectList, InferredSchema *schema);
extern MongoCollection *MapTableToCollection(const char *tableName);

/* Result conversion functions */
extern TupleTableSlot *ConvertBSONToSQLTuple(pgbson *document, 
											 InferredSchema *schema,
											 TupleTableSlot *slot);

/* Utility functions */
extern void InitializeSQLQueryRouter(void);
extern void CleanupSQLQueryRouter(void);

#endif /* SQL_QUERY_ROUTER_H */
