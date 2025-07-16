/*-------------------------------------------------------------------------
 * Copyright (c) Microsoft Corporation.  All rights reserved.
 *
 * src/oss_backend/commands/bulk_write.c
 *
 * Implementation of the bulkWrite operation.
 *
 *-------------------------------------------------------------------------
 */
#include <postgres.h>
#include <fmgr.h>
#include <miscadmin.h>
#include <funcapi.h>
#include <nodes/makefuncs.h>
#include <utils/timestamp.h>
#include <utils/portal.h>
#include <tcop/dest.h>
#include <tcop/pquery.h>
#include <tcop/tcopprot.h>
#include <commands/portalcmds.h>
#include <utils/snapmgr.h>
#include <catalog/pg_class.h>
#include <parser/parse_relation.h>
#include <utils/lsyscache.h>

#include "access/xact.h"
#include "executor/spi.h"
#include "lib/stringinfo.h"
#include "utils/builtins.h"

#include "io/bson_core.h"
#include "commands/commands_common.h"
#include "commands/bulk_write.h"
#include "commands/insert.h"
#include "commands/update.h"
#include "commands/delete.h"
#include "commands/parse_error.h"
#include "metadata/collection.h"
#include "infrastructure/documentdb_plan_cache.h"
#include "sharding/sharding.h"
#include "commands/retryable_writes.h"
#include "io/pgbsonsequence.h"
#include "utils/query_utils.h"
#include "utils/feature_counter.h"
#include "metadata/metadata_cache.h"
#include "utils/error_utils.h"
#include "utils/version_utils.h"
#include "utils/documentdb_errors.h"
#include "api_hooks.h"
#include "schema_validation/schema_validation.h"
#include "operators/bson_expr_eval.h"

/*
 * Forward declarations of structures from other command files
 */
typedef struct
{
	UpdateOneParams updateOneParams;
	int isMulti;
} UpdateSpec;

typedef struct
{
	uint64 rowsModified;
	uint64 rowsMatched;
	bool performedUpsert;
	pgbson *upsertedObjectId;
} UpdateResult;

typedef struct
{
	DeleteOneParams deleteOneParams;
	int limit;
} DeletionSpec;

PG_FUNCTION_INFO_V1(command_bulk_write);

static BulkWriteSpec * BuildBulkWriteSpec(bson_iter_t *bulkWriteCommandIter,
										   pgbsonsequence *bulkOperations);
static List * BuildBulkOperationList(bson_iter_t *operationsArrayIter);
static BulkWriteOperation * BuildBulkWriteOperation(bson_iter_t *operationIter, int operationIndex);
static void ProcessBulkWrite(MongoCollection *collection, BulkWriteSpec *bulkSpec,
							 text *transactionId, BulkWriteResult *bulkResult);
static bool ProcessSingleBulkOperation(MongoCollection *collection,
									   BulkWriteOperation *operation,
									   text *transactionId,
									   BulkWriteResult *bulkResult,
									   int operationIndex);
static pgbson * CreateBulkWriteResultDocument(BulkWriteResult *bulkResult);
static void AddWriteErrorToBulkResult(BulkWriteResult *bulkResult, int operationIndex,
									  int errorCode, const char *errorMessage);

/* External function declarations from other command files */
extern uint64 ProcessInsertion(MongoCollection *collection, Oid optionalInsertShardOid,
							   const bson_value_t *documentValue, text *transactionId,
							   ExprEvalState *evalState);
extern void ProcessUpdate(MongoCollection *collection, UpdateSpec *updateSpec,
						  text *transactionId, UpdateResult *result, bool forceInlineWrites,
						  ExprEvalState *stateForSchemaValidation);
extern uint64 ProcessDeletion(MongoCollection *collection, DeletionSpec *deletionSpec,
							  bool forceInlineWrites, text *transactionId);

/*
 * command_bulk_write processes a MongoDB bulkWrite command.
 */
Datum
command_bulk_write(PG_FUNCTION_ARGS)
{
	text *databaseName = PG_GETARG_TEXT_P(0);
	pgbson *bulkWriteCommand = PG_GETARG_PGBSON(1);
	pgbsonsequence *bulkOperations = PG_ARGISNULL(2) ? NULL : PG_GETARG_PGBSON_SEQUENCE(2);
	text *transactionId = PG_ARGISNULL(3) ? NULL : PG_GETARG_TEXT_P(3);

	bson_iter_t bulkWriteCommandIter;
	PgbsonInitIterator(bulkWriteCommand, &bulkWriteCommandIter);

	BulkWriteSpec *bulkSpec = BuildBulkWriteSpec(&bulkWriteCommandIter, bulkOperations);

	char *databaseNameString = text_to_cstring(databaseName);
	MongoCollection *collection = GetMongoCollectionByNameDatum(
		CStringGetTextDatum(databaseNameString), CStringGetTextDatum(bulkSpec->collectionName), NoLock);

	if (collection == NULL)
	{
		ereport(ERROR, (errcode(ERRCODE_DOCUMENTDB_NAMESPACENOTFOUND),
						errmsg("ns not found: %s.%s", databaseNameString, bulkSpec->collectionName)));
	}

	BulkWriteResult *bulkResult = palloc0(sizeof(BulkWriteResult));
	bulkResult->resultMemoryContext = CurrentMemoryContext;

	ProcessBulkWrite(collection, bulkSpec, transactionId, bulkResult);

	pgbson *resultDocument = CreateBulkWriteResultDocument(bulkResult);

	Datum values[2];
	bool nulls[2] = { false, false };

	values[0] = PointerGetDatum(resultDocument);
	values[1] = BoolGetDatum(true);

	TupleDesc tupleDescriptor = CreateTemplateTupleDesc(2);
	TupleDescInitEntry(tupleDescriptor, (AttrNumber) 1, "p_result",
					   DocumentDBCoreBsonTypeId(), -1, 0);
	TupleDescInitEntry(tupleDescriptor, (AttrNumber) 2, "p_success",
					   BOOLOID, -1, 0);

	tupleDescriptor = BlessTupleDesc(tupleDescriptor);
	HeapTuple resultTuple = heap_form_tuple(tupleDescriptor, values, nulls);

	PG_RETURN_DATUM(HeapTupleGetDatum(resultTuple));
}

/*
 * BuildBulkWriteSpec parses the bulkWrite command and builds a BulkWriteSpec.
 */
static BulkWriteSpec *
BuildBulkWriteSpec(bson_iter_t *bulkWriteCommandIter, pgbsonsequence *bulkOperations)
{
	BulkWriteSpec *bulkSpec = palloc0(sizeof(BulkWriteSpec));
	bulkSpec->isOrdered = true;
	bulkSpec->bypassDocumentValidation = false;

	while (bson_iter_next(bulkWriteCommandIter))
	{
		const char *key = bson_iter_key(bulkWriteCommandIter);

		if (strcmp(key, "bulkWrite") == 0)
		{
			if (BSON_ITER_HOLDS_UTF8(bulkWriteCommandIter))
			{
				const char *collectionName = bson_iter_utf8(bulkWriteCommandIter, NULL);
				bulkSpec->collectionName = pstrdup(collectionName);
			}
		}
		else if (strcmp(key, "ops") == 0)
		{
			if (BSON_ITER_HOLDS_ARRAY(bulkWriteCommandIter))
			{
				bson_iter_t operationsArrayIter;
				bson_iter_recurse(bulkWriteCommandIter, &operationsArrayIter);
				bulkSpec->operations = BuildBulkOperationList(&operationsArrayIter);
			}
		}
		else if (strcmp(key, "ordered") == 0)
		{
			if (BSON_ITER_HOLDS_BOOL(bulkWriteCommandIter))
			{
				bulkSpec->isOrdered = bson_iter_bool(bulkWriteCommandIter);
			}
		}
		else if (strcmp(key, "bypassDocumentValidation") == 0)
		{
			if (BSON_ITER_HOLDS_BOOL(bulkWriteCommandIter))
			{
				bulkSpec->bypassDocumentValidation = bson_iter_bool(bulkWriteCommandIter);
			}
		}
		else if (!IsCommonSpecIgnoredField(key))
		{
			ereport(ERROR, (errcode(ERRCODE_DOCUMENTDB_BADVALUE),
							errmsg("BSON field 'bulkWrite.%s' is an unknown field.", key)));
		}
	}

	if (bulkSpec->collectionName == NULL)
	{
		ereport(ERROR, (errcode(ERRCODE_DOCUMENTDB_BADVALUE),
						errmsg("BSON field 'bulkWrite' is missing but a required field")));
	}

	if (bulkSpec->operations == NIL)
	{
		ereport(ERROR, (errcode(ERRCODE_DOCUMENTDB_BADVALUE),
						errmsg("BSON field 'ops' is missing but a required field")));
	}

	return bulkSpec;
}

/*
 * BuildBulkOperationList parses the operations array and builds a list of BulkWriteOperation.
 */
static List *
BuildBulkOperationList(bson_iter_t *operationsArrayIter)
{
	List *operations = NIL;
	int operationIndex = 0;

	while (bson_iter_next(operationsArrayIter))
	{
		if (BSON_ITER_HOLDS_DOCUMENT(operationsArrayIter))
		{
			bson_iter_t operationIter;
			bson_iter_recurse(operationsArrayIter, &operationIter);
			BulkWriteOperation *operation = BuildBulkWriteOperation(&operationIter, operationIndex);
			operations = lappend(operations, operation);
		}
		operationIndex++;
	}

	return operations;
}

/*
 * BuildBulkWriteOperation parses a single operation and builds a BulkWriteOperation.
 */
static BulkWriteOperation *
BuildBulkWriteOperation(bson_iter_t *operationIter, int operationIndex)
{
	BulkWriteOperation *operation = palloc0(sizeof(BulkWriteOperation));
	operation->operationIndex = operationIndex;
	operation->type = BULK_WRITE_UNKNOWN;

	while (bson_iter_next(operationIter))
	{
		const char *key = bson_iter_key(operationIter);

		if (strcmp(key, "insertOne") == 0)
		{
			operation->type = BULK_WRITE_INSERT_ONE;
			bson_value_copy(bson_iter_value(operationIter), &operation->operationSpec);
		}
		else if (strcmp(key, "updateOne") == 0)
		{
			operation->type = BULK_WRITE_UPDATE_ONE;
			bson_value_copy(bson_iter_value(operationIter), &operation->operationSpec);
		}
		else if (strcmp(key, "updateMany") == 0)
		{
			operation->type = BULK_WRITE_UPDATE_MANY;
			bson_value_copy(bson_iter_value(operationIter), &operation->operationSpec);
		}
		else if (strcmp(key, "replaceOne") == 0)
		{
			operation->type = BULK_WRITE_REPLACE_ONE;
			bson_value_copy(bson_iter_value(operationIter), &operation->operationSpec);
		}
		else if (strcmp(key, "deleteOne") == 0)
		{
			operation->type = BULK_WRITE_DELETE_ONE;
			bson_value_copy(bson_iter_value(operationIter), &operation->operationSpec);
		}
		else if (strcmp(key, "deleteMany") == 0)
		{
			operation->type = BULK_WRITE_DELETE_MANY;
			bson_value_copy(bson_iter_value(operationIter), &operation->operationSpec);
		}
	}

	if (operation->type == BULK_WRITE_UNKNOWN)
	{
		ereport(ERROR, (errcode(ERRCODE_DOCUMENTDB_BADVALUE),
						errmsg("bulkWrite operation at index %d is not a valid operation type", operationIndex)));
	}

	return operation;
}

/*
 * ProcessBulkWrite processes all operations in the bulk write request.
 */
static void
ProcessBulkWrite(MongoCollection *collection, BulkWriteSpec *bulkSpec,
				 text *transactionId, BulkWriteResult *bulkResult)
{
	bulkResult->ok = 1.0;
	bulkResult->insertedCount = 0;
	bulkResult->matchedCount = 0;
	bulkResult->modifiedCount = 0;
	bulkResult->deletedCount = 0;
	bulkResult->upsertedCount = 0;
	bulkResult->upsertedIds = NIL;
	bulkResult->writeErrors = NIL;

	ListCell *operationCell = NULL;
	int operationIndex = 0;

	foreach(operationCell, bulkSpec->operations)
	{
		CHECK_FOR_INTERRUPTS();

		BulkWriteOperation *operation = lfirst(operationCell);
		bool isSuccess = ProcessSingleBulkOperation(collection, operation,
													transactionId, bulkResult,
													operationIndex);

		if (!isSuccess && bulkSpec->isOrdered)
		{
			break;
		}

		operationIndex++;
	}
}

/*
 * ProcessSingleBulkOperation processes a single operation in the bulk write request.
 */
static bool
ProcessSingleBulkOperation(MongoCollection *collection,
						   BulkWriteOperation *operation,
						   text *transactionId,
						   BulkWriteResult *bulkResult,
						   int operationIndex)
{
	volatile bool isSuccess = true;

	PG_TRY();
	{
		switch (operation->type)
		{
			case BULK_WRITE_INSERT_ONE:
			{
				bson_iter_t operationIter;
				BsonValueInitIterator(&operation->operationSpec, &operationIter);
				
				while (bson_iter_next(&operationIter))
				{
					const char *key = bson_iter_key(&operationIter);
					if (strcmp(key, "document") == 0)
					{
						const bson_value_t *documentValue = bson_iter_value(&operationIter);
						uint64 insertedRows = ProcessInsertion(collection, InvalidOid, 
															   documentValue, transactionId, NULL);
						bulkResult->insertedCount += insertedRows;
						break;
					}
				}
				break;
			}
			case BULK_WRITE_UPDATE_ONE:
			case BULK_WRITE_UPDATE_MANY:
			case BULK_WRITE_REPLACE_ONE:
			{
				bson_iter_t operationIter;
				BsonValueInitIterator(&operation->operationSpec, &operationIter);
				
				UpdateOneParams updateParams = { 0 };
				bool isMulti = (operation->type == BULK_WRITE_UPDATE_MANY);
				
				while (bson_iter_next(&operationIter))
				{
					const char *key = bson_iter_key(&operationIter);
					if (strcmp(key, "filter") == 0)
					{
						updateParams.query = bson_iter_value(&operationIter);
					}
					else if (strcmp(key, "update") == 0 || strcmp(key, "replacement") == 0)
					{
						updateParams.update = bson_iter_value(&operationIter);
					}
					else if (strcmp(key, "upsert") == 0)
					{
						updateParams.isUpsert = bson_iter_bool(&operationIter);
					}
					else if (strcmp(key, "arrayFilters") == 0)
					{
						updateParams.arrayFilters = bson_iter_value(&operationIter);
					}
				}
				
				UpdateSpec updateSpec = { 0 };
				updateSpec.updateOneParams = updateParams;
				updateSpec.isMulti = isMulti;
				
				UpdateResult updateResult = { 0 };
				ProcessUpdate(collection, &updateSpec, transactionId, &updateResult, false, NULL);
				
				bulkResult->matchedCount += updateResult.rowsMatched;
				bulkResult->modifiedCount += updateResult.rowsModified;
				if (updateResult.performedUpsert)
				{
					bulkResult->upsertedCount++;
				}
				break;
			}
			case BULK_WRITE_DELETE_ONE:
			case BULK_WRITE_DELETE_MANY:
			{
				bson_iter_t operationIter;
				BsonValueInitIterator(&operation->operationSpec, &operationIter);
				
				DeleteOneParams deleteParams = { 0 };
				int limit = (operation->type == BULK_WRITE_DELETE_ONE) ? 1 : 0;
				
				while (bson_iter_next(&operationIter))
				{
					const char *key = bson_iter_key(&operationIter);
					if (strcmp(key, "filter") == 0)
					{
						deleteParams.query = bson_iter_value(&operationIter);
					}
				}
				
				DeletionSpec deletionSpec = { 0 };
				deletionSpec.deleteOneParams = deleteParams;
				deletionSpec.limit = limit;
				
				uint64 deletedRows = ProcessDeletion(collection, &deletionSpec, false, transactionId);
				bulkResult->deletedCount += deletedRows;
				break;
			}
			default:
				ereport(ERROR, (errcode(ERRCODE_DOCUMENTDB_BADVALUE),
								errmsg("Unknown bulk write operation type")));
		}
	}
	PG_CATCH();
	{
		ErrorData *errorData = CopyErrorData();
		AddWriteErrorToBulkResult(bulkResult, operationIndex,
								  errorData->sqlerrcode, errorData->message);
		FlushErrorState();
		isSuccess = false;
	}
	PG_END_TRY();

	return isSuccess;
}

/*
 * CreateBulkWriteResultDocument creates the result document for the bulk write operation.
 */
static pgbson *
CreateBulkWriteResultDocument(BulkWriteResult *bulkResult)
{
	pgbson_writer writer;
	PgbsonWriterInit(&writer);

	PgbsonWriterAppendDouble(&writer, "ok", 2, bulkResult->ok);
	PgbsonWriterAppendInt64(&writer, "insertedCount", 13, bulkResult->insertedCount);
	PgbsonWriterAppendInt64(&writer, "matchedCount", 12, bulkResult->matchedCount);
	PgbsonWriterAppendInt64(&writer, "modifiedCount", 13, bulkResult->modifiedCount);
	PgbsonWriterAppendInt64(&writer, "deletedCount", 12, bulkResult->deletedCount);
	PgbsonWriterAppendInt64(&writer, "upsertedCount", 13, bulkResult->upsertedCount);

	if (bulkResult->upsertedIds != NIL)
	{
		pgbson_array_writer arrayWriter;
		PgbsonWriterStartArray(&writer, "upsertedIds", 11, &arrayWriter);
		PgbsonWriterEndArray(&writer, &arrayWriter);
	}

	if (bulkResult->writeErrors != NIL)
	{
		pgbson_array_writer arrayWriter;
		PgbsonWriterStartArray(&writer, "writeErrors", 11, &arrayWriter);
		ListCell *errorCell = NULL;
		foreach(errorCell, bulkResult->writeErrors)
		{
			WriteError *writeError = lfirst(errorCell);
			pgbson_writer docWriter;
			PgbsonWriterStartDocument(&writer, NULL, 0, &docWriter);
			PgbsonWriterAppendInt32(&docWriter, "index", 5, writeError->index);
			PgbsonWriterAppendInt32(&docWriter, "code", 4, writeError->code);
			PgbsonWriterAppendUtf8(&docWriter, "errmsg", 6, writeError->errmsg);
			PgbsonWriterEndDocument(&writer, &docWriter);
		}
		PgbsonWriterEndArray(&writer, &arrayWriter);
	}

	return PgbsonWriterGetPgbson(&writer);
}

/*
 * AddWriteErrorToBulkResult adds a write error to the bulk result.
 */
static void
AddWriteErrorToBulkResult(BulkWriteResult *bulkResult, int operationIndex,
						  int errorCode, const char *errorMessage)
{
	MemoryContext oldContext = MemoryContextSwitchTo(bulkResult->resultMemoryContext);

	WriteError *writeError = palloc0(sizeof(WriteError));
	writeError->index = operationIndex;
	writeError->code = errorCode;
	writeError->errmsg = pstrdup(errorMessage);

	bulkResult->writeErrors = lappend(bulkResult->writeErrors, writeError);

	MemoryContextSwitchTo(oldContext);
}
