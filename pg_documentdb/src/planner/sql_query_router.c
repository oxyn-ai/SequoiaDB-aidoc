/*-------------------------------------------------------------------------
 * Copyright (c) Microsoft Corporation.  All rights reserved.
 *
 * src/planner/sql_query_router.c
 *
 * SQL Query Router implementation for DocumentDB
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "optimizer/planner.h"
#include "parser/parse_relation.h"
#include "tcop/params.h"
#include "utils/builtins.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"
#include "utils/timestamp.h"

#include "planner/sql_query_router.h"
#include "planner/documentdb_planner.h"
#include "metadata/collection.h"
#include "metadata/metadata_cache.h"
#include "io/bson_core.h"
#include "executor/spi.h"
#include "catalog/namespace.h"
#include "utils/lsyscache.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"
#include "executor/executor.h"
#include "nodes/parsenodes.h"
#include "nodes/primnodes.h"
#include "parser/parse_node.h"
#include "catalog/pg_type.h"

/* External variables from documentdb_planner.c */
extern planner_hook_type ExtensionPreviousPlannerHook;

/* Constants */
#define DOCUMENT_DATA_SCHEMA_NAME "documentdb_data"

/* Global schema cache hash table */
static HTAB *SchemaCache = NULL;
static MemoryContext SchemaCacheContext = NULL;

/* Schema cache entry structure */
typedef struct SchemaCacheEntry
{
	uint64 collectionId;          /* Hash key */
	InferredSchema *schema;       /* Cached schema */
	Timestamp cacheTime;          /* Cache creation time */
	int accessCount;              /* Access frequency counter */
} SchemaCacheEntry;

/* Forward declarations */
static bool IsDocumentDBTableName(const char *tableName);
static SQLQueryInfo *AllocateSQLQueryInfo(void);
static void FreeSQLQueryInfo(SQLQueryInfo *queryInfo);
static InferredSchema *AllocateInferredSchema(int fieldCount);
static void FreeInferredSchema(InferredSchema *schema);
static bool WalkQueryTreeForMongoFunctions(Node *node, void *context);

/*
 * IdentifyQueryType - Determine the type of query for proper routing
 */
QueryType
IdentifyQueryType(Query *parse, const char *queryString)
{
	/* Check if query accesses documentdb_data schema tables */
	if (IsDocumentDBDataSchemaQuery(parse))
	{
		return QUERY_TYPE_SQL_ON_DOCUMENTS;
	}

	/* Check if query contains MongoDB-specific functions */
	if (ContainsMongoDBFunctions(parse))
	{
		return QUERY_TYPE_MONGODB;
	}

	/* Default to standard SQL */
	return QUERY_TYPE_STANDARD_SQL;
}

/*
 * IsDocumentDBDataSchemaQuery - Check if query accesses documentdb_data schema
 */
bool
IsDocumentDBDataSchemaQuery(Query *parse)
{
	ListCell *lc;

	if (parse->rtable == NIL)
		return false;

	foreach(lc, parse->rtable)
	{
		RangeTblEntry *rte = (RangeTblEntry *) lfirst(lc);
		
		if (rte->rtekind == RTE_RELATION)
		{
			char *tableName = get_rel_name(rte->relid);
			if (tableName && IsDocumentDBTableName(tableName))
			{
				return true;
			}
		}
	}

	return false;
}

/*
 * IsDocumentDBTableName - Check if table name follows DocumentDB naming pattern
 */
static bool
IsDocumentDBTableName(const char *tableName)
{
	if (tableName == NULL)
		return false;

	/* Check for documents_ prefix pattern */
	return strncmp(tableName, DOCUMENT_DATA_TABLE_NAME_PREFIX, 
				   strlen(DOCUMENT_DATA_TABLE_NAME_PREFIX)) == 0;
}

/*
 * ContainsMongoDBFunctions - Check if query contains MongoDB-specific functions
 */
bool
ContainsMongoDBFunctions(Query *parse)
{
	return query_tree_walker(parse, WalkQueryTreeForMongoFunctions, NULL, 0);
}

/*
 * WalkQueryTreeForMongoFunctions - Walker function to detect MongoDB functions
 */
static bool
WalkQueryTreeForMongoFunctions(Node *node, void *context)
{
	if (node == NULL)
		return false;

	if (IsA(node, FuncExpr))
	{
		FuncExpr *funcExpr = (FuncExpr *) node;
		char *funcName = get_func_name(funcExpr->funcid);
		
		if (funcName)
		{
			/* Check for known MongoDB function patterns */
			if (strstr(funcName, "bson_") != NULL ||
				strstr(funcName, "mongo_") != NULL ||
				strstr(funcName, "documentdb_") != NULL)
			{
				return true;
			}
		}
	}

	return expression_tree_walker(node, WalkQueryTreeForMongoFunctions, context);
}

/*
 * ProcessSQLOnDocumentsQuery - Main handler for SQL queries on document collections
 */
PlannedStmt *
ProcessSQLOnDocumentsQuery(Query *parse, const char *queryString,
						   int cursorOptions, ParamListInfo boundParams)
{
	SQLQueryInfo *sqlInfo;
	MongoCollection *collection;
	InferredSchema *schema;
	pgbson *bsonQuery = NULL;
	pgbson *bsonProjection = NULL;
	PlannedStmt *plan;

	/* Parse the SQL query structure */
	sqlInfo = ParseSQLQuery(parse);
	if (sqlInfo == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_SQL_STATEMENT_NAME),
				 errmsg("Failed to parse SQL query structure")));
	}

	/* Map table name to MongoDB collection */
	collection = MapTableToCollection(sqlInfo->tableName);
	if (collection == NULL)
	{
		FreeSQLQueryInfo(sqlInfo);
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_TABLE),
				 errmsg("Table \"%s\" does not correspond to a valid DocumentDB collection",
						sqlInfo->tableName)));
	}

	/* Get or infer schema for the collection */
	schema = GetCachedSchema(collection->collectionId);
	if (schema == NULL)
	{
		schema = InferSchemaFromCollection(collection);
		if (schema != NULL)
		{
			UpdateSchemaCache(collection->collectionId, schema);
		}
	}

	if (schema == NULL)
	{
		FreeSQLQueryInfo(sqlInfo);
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Failed to infer schema for collection \"%s\"",
						collection->name.collectionName)));
	}

	/* Convert SQL WHERE clause to BSON query */
	if (sqlInfo->whereClause != NULL)
	{
		bsonQuery = ConvertSQLWhereToBSON(sqlInfo->whereClause, schema);
	}

	/* Convert SQL SELECT list to BSON projection */
	if (sqlInfo->selectList != NIL)
	{
		bsonProjection = ConvertSQLSelectToBSON(sqlInfo->selectList, schema);
	}

	/* Generate MongoDB-style query plan */
	/* For now, fall back to standard planner with modifications */
	plan = standard_planner(parse, queryString, cursorOptions, boundParams);

	/* TODO: Integrate BSON query and projection into the plan */
	/* This would involve creating custom scan nodes or modifying existing ones */

	FreeSQLQueryInfo(sqlInfo);
	return plan;
}

/*
 * ParseSQLQuery - Extract key components from parsed SQL query
 */
SQLQueryInfo *
ParseSQLQuery(Query *parse)
{
	SQLQueryInfo *queryInfo;
	ListCell *lc;

	if (parse == NULL)
		return NULL;

	queryInfo = AllocateSQLQueryInfo();

	/* Extract table name from first RTE */
	if (parse->rtable != NIL)
	{
		RangeTblEntry *rte = (RangeTblEntry *) linitial(parse->rtable);
		if (rte->rtekind == RTE_RELATION)
		{
			char *tableName = get_rel_name(rte->relid);
			if (tableName)
			{
				queryInfo->tableName = pstrdup(tableName);
			}
		}
	}

	/* Copy query components */
	queryInfo->selectList = list_copy(parse->targetList);
	queryInfo->whereClause = copyObject(parse->jointree->quals);
	queryInfo->fromClause = list_copy(parse->rtable);
	queryInfo->groupClause = list_copy(parse->groupClause);
	queryInfo->havingClause = copyObject(parse->havingQual);
	queryInfo->orderClause = list_copy(parse->sortClause);
	queryInfo->limitClause = copyObject(parse->limitCount);

	return queryInfo;
}

/*
 * MapTableToCollection - Map PostgreSQL table name to MongoDB collection
 */
MongoCollection *
MapTableToCollection(const char *tableName)
{
	uint64 collectionId;
	char *endptr;

	if (tableName == NULL || !IsDocumentDBTableName(tableName))
		return NULL;

	/* Extract collection ID from table name (documents_12345 -> 12345) */
	const char *idStr = tableName + strlen(DOCUMENT_DATA_TABLE_NAME_PREFIX);
	collectionId = strtoull(idStr, &endptr, 10);
	
	if (*endptr != '\0' || collectionId == 0)
		return NULL;

	/* Get collection metadata by ID */
	return GetMongoCollectionByColId(collectionId, AccessShareLock);
}

/*
 * Schema cache management functions
 */

/*
 * InitializeSQLQueryRouter - Initialize the SQL query router subsystem
 */
void
InitializeSQLQueryRouter(void)
{
	HASHCTL info;

	/* Create memory context for schema cache */
	SchemaCacheContext = AllocSetContextCreate(TopMemoryContext,
											   "SQL Query Router Schema Cache",
											   ALLOCSET_DEFAULT_SIZES);

	/* Initialize schema cache hash table */
	MemSet(&info, 0, sizeof(info));
	info.keysize = sizeof(uint64);
	info.entrysize = sizeof(SchemaCacheEntry);
	info.hcxt = SchemaCacheContext;

	SchemaCache = hash_create("DocumentDB Schema Cache",
							  32,
							  &info,
							  HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
}

/*
 * GetCachedSchema - Retrieve schema from cache
 */
InferredSchema *
GetCachedSchema(uint64 collectionId)
{
	SchemaCacheEntry *entry;
	bool found;

	if (SchemaCache == NULL)
		InitializeSQLQueryRouter();

	entry = (SchemaCacheEntry *) hash_search(SchemaCache,
											 &collectionId,
											 HASH_FIND,
											 &found);

	if (found)
	{
		entry->accessCount++;
		return entry->schema;
	}

	return NULL;
}

/*
 * UpdateSchemaCache - Update or insert schema in cache
 */
void
UpdateSchemaCache(uint64 collectionId, InferredSchema *schema)
{
	SchemaCacheEntry *entry;
	bool found;

	if (SchemaCache == NULL)
		InitializeSQLQueryRouter();

	entry = (SchemaCacheEntry *) hash_search(SchemaCache,
											 &collectionId,
											 HASH_ENTER,
											 &found);

	if (found && entry->schema != NULL)
	{
		FreeInferredSchema(entry->schema);
	}

	entry->schema = schema;
	entry->cacheTime = GetCurrentTimestamp();
	entry->accessCount = 1;
}

/*
 * InvalidateSchemaCache - Remove schema from cache
 */
void
InvalidateSchemaCache(uint64 collectionId)
{
	SchemaCacheEntry *entry;
	bool found;

	if (SchemaCache == NULL)
		return;

	entry = (SchemaCacheEntry *) hash_search(SchemaCache,
											 &collectionId,
											 HASH_REMOVE,
											 &found);

	if (found && entry->schema != NULL)
	{
		FreeInferredSchema(entry->schema);
	}
}

/*
 * Memory management helper functions
 */

static SQLQueryInfo *
AllocateSQLQueryInfo(void)
{
	SQLQueryInfo *queryInfo = (SQLQueryInfo *) palloc0(sizeof(SQLQueryInfo));
	return queryInfo;
}

static void
FreeSQLQueryInfo(SQLQueryInfo *queryInfo)
{
	if (queryInfo == NULL)
		return;

	if (queryInfo->tableName)
		pfree(queryInfo->tableName);

	/* Lists and nodes will be freed by PostgreSQL's memory context cleanup */
	pfree(queryInfo);
}

static InferredSchema *
AllocateInferredSchema(int fieldCount)
{
	InferredSchema *schema = (InferredSchema *) palloc0(sizeof(InferredSchema));
	
	if (fieldCount > 0)
	{
		schema->mappings = (BSONPathMapping *) palloc0(fieldCount * sizeof(BSONPathMapping));
	}
	
	schema->fieldCount = fieldCount;
	return schema;
}

static void
FreeInferredSchema(InferredSchema *schema)
{
	if (schema == NULL)
		return;

	if (schema->mappings != NULL)
	{
		for (int i = 0; i < schema->fieldCount; i++)
		{
			if (schema->mappings[i].sqlColumn)
				pfree(schema->mappings[i].sqlColumn);
			if (schema->mappings[i].bsonPath)
				pfree(schema->mappings[i].bsonPath);
		}
		pfree(schema->mappings);
	}

	pfree(schema);
}

/*
 * CleanupSQLQueryRouter - Cleanup resources
 */
void
CleanupSQLQueryRouter(void)
{
	if (SchemaCacheContext != NULL)
	{
		MemoryContextDelete(SchemaCacheContext);
		SchemaCacheContext = NULL;
		SchemaCache = NULL;
	}
}

/*
 * Stub implementations for functions that need to be implemented
 */

InferredSchema *
InferSchemaFromCollection(MongoCollection *collection)
{
	InferredSchema *schema;
	char *queryString;
	SPITupleTable *tuptable;
	int ret;
	int processed;

	if (collection == NULL)
		return NULL;

	/* Connect to SPI for executing queries */
	if (SPI_connect() != SPI_OK_CONNECT)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Failed to connect to SPI for schema inference")));
	}

	/* Query to sample documents from the collection */
	queryString = psprintf("SELECT document FROM %s.%s LIMIT 100",
						   get_namespace_name(collection->schemaId),
						   collection->tableName);

	ret = SPI_execute(queryString, true, 100);
	if (ret != SPI_OK_SELECT)
	{
		SPI_finish();
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Failed to sample documents for schema inference")));
	}

	processed = SPI_processed;
	tuptable = SPI_tuptable;

	if (processed == 0)
	{
		SPI_finish();
		/* Return empty schema for collections with no documents */
		schema = AllocateInferredSchema(0);
		schema->collectionId = collection->collectionId;
		schema->lastUpdated = GetCurrentTimestamp();
		schema->sampleSize = 0;
		return schema;
	}

	/* For now, create a basic schema with common BSON fields */
	/* This is a simplified implementation - in production, we would analyze the BSON structure */
	schema = AllocateInferredSchema(3);
	schema->collectionId = collection->collectionId;
	schema->lastUpdated = GetCurrentTimestamp();
	schema->sampleSize = processed;

	/* Add basic mappings for common fields */
	schema->mappings[0].sqlColumn = pstrdup("_id");
	schema->mappings[0].bsonPath = pstrdup("_id");
	schema->mappings[0].sqlType = TEXTOID;
	schema->mappings[0].isNullable = false;

	schema->mappings[1].sqlColumn = pstrdup("shard_key_value");
	schema->mappings[1].bsonPath = pstrdup("shard_key_value");
	schema->mappings[1].sqlType = INT8OID;
	schema->mappings[1].isNullable = true;

	schema->mappings[2].sqlColumn = pstrdup("document");
	schema->mappings[2].bsonPath = pstrdup("");
	schema->mappings[2].sqlType = BsonTypeId();
	schema->mappings[2].isNullable = false;

	SPI_finish();
	pfree(queryString);

	return schema;
}

pgbson *
ConvertSQLWhereToBSON(Node *whereClause, InferredSchema *schema)
{
	pgbson_writer writer;
	pgbson *result;

	if (whereClause == NULL)
	{
		/* No WHERE clause - return empty BSON query (matches all) */
		PgbsonWriterInit(&writer);
		result = PgbsonWriterGetPgbson(&writer);
		return result;
	}

	/* Initialize BSON writer for query construction */
	PgbsonWriterInit(&writer);

	/* For now, implement basic equality conditions */
	/* This is a simplified implementation - full SQL WHERE conversion would be much more complex */
	if (IsA(whereClause, OpExpr))
	{
		OpExpr *opExpr = (OpExpr *) whereClause;
		
		/* Handle simple equality operations */
		if (list_length(opExpr->args) == 2)
		{
			Node *leftArg = (Node *) linitial(opExpr->args);
			Node *rightArg = (Node *) lsecond(opExpr->args);
			
			if (IsA(leftArg, Var) && IsA(rightArg, Const))
			{
				Var *var = (Var *) leftArg;
				Const *constVal = (Const *) rightArg;
				
				/* For now, skip complex WHERE clause conversion */
				/* TODO: Implement proper column name resolution and BSON field mapping */
			}
		}
	}

	result = PgbsonWriterGetPgbson(&writer);
	return result;
}

pgbson *
ConvertSQLSelectToBSON(List *selectList, InferredSchema *schema)
{
	pgbson_writer writer;
	pgbson *result;
	ListCell *lc;

	if (selectList == NIL)
	{
		/* No SELECT list - return NULL for no projection (select all) */
		return NULL;
	}

	/* Initialize BSON writer for projection construction */
	PgbsonWriterInit(&writer);

	/* Convert each target entry to BSON projection */
	foreach(lc, selectList)
	{
		TargetEntry *tle = (TargetEntry *) lfirst(lc);
		
		if (tle->resjunk)
			continue;

		/* Handle simple column references */
		if (IsA(tle->expr, Var))
		{
			/* For now, include all fields in projection */
			/* TODO: Implement proper column name resolution and BSON field mapping */
			if (tle->resname)
			{
				PgbsonWriterAppendInt32(&writer, tle->resname, 1);
			}
		}
		else if (tle->resname)
		{
			/* For expressions with result names, include in projection */
			PgbsonWriterAppendInt32(&writer, tle->resname, 1);
		}
	}

	result = PgbsonWriterGetPgbson(&writer);
	return result;
}

TupleTableSlot *
ConvertBSONToSQLTuple(pgbson *document, InferredSchema *schema, TupleTableSlot *slot)
{
	pgbson_iterator iterator;
	pgbson_iterator_init(&iterator, document);

	if (schema == NULL || slot == NULL)
		return slot;

	/* Clear the slot */
	ExecClearTuple(slot);

	/* Iterate through schema mappings and extract values from BSON */
	for (int i = 0; i < schema->fieldCount; i++)
	{
		BSONPathMapping *mapping = &schema->mappings[i];
		bool isNull = true;
		Datum value = (Datum) 0;

		/* Reset iterator for each field */
		pgbson_iterator_init(&iterator, document);

		/* Find the field in the BSON document */
		if (strlen(mapping->bsonPath) == 0)
		{
			/* Empty path means the entire document */
			value = PgbsonToDatum(document);
			isNull = false;
		}
		else
		{
			/* Look for the specific field */
			while (pgbson_iterator_next(&iterator))
			{
				const char *fieldName = pgbson_iterator_key(&iterator);
				if (strcmp(fieldName, mapping->bsonPath) == 0)
				{
					/* Found the field - convert based on type */
					switch (mapping->sqlType)
					{
						case TEXTOID:
						case VARCHAROID:
						{
							if (pgbson_iterator_type(&iterator) == BSON_TYPE_UTF8)
							{
								const char *strVal = pgbson_iterator_utf8(&iterator, NULL);
								value = CStringGetTextDatum(strVal);
								isNull = false;
							}
							break;
						}
						case INT4OID:
						{
							if (pgbson_iterator_type(&iterator) == BSON_TYPE_INT32)
							{
								int32 intVal = pgbson_iterator_int32(&iterator);
								value = Int32GetDatum(intVal);
								isNull = false;
							}
							break;
						}
						case INT8OID:
						{
							if (pgbson_iterator_type(&iterator) == BSON_TYPE_INT64)
							{
								int64 longVal = pgbson_iterator_int64(&iterator);
								value = Int64GetDatum(longVal);
								isNull = false;
							}
							break;
						}
						default:
						{
							/* For BSON type, store the entire document */
							value = PgbsonToDatum(document);
							isNull = false;
							break;
						}
					}
					break;
				}
			}
		}

		/* Set the value in the tuple slot */
		slot->tts_values[i] = value;
		slot->tts_isnull[i] = isNull;
	}

	/* Mark slot as containing data */
	ExecStoreVirtualTuple(slot);
	return slot;
}
