/*-------------------------------------------------------------------------
 * Copyright (c) Microsoft Corporation.  All rights reserved.
 *
 * include/commands/bulk_write.h
 *
 * Declarations for bulk write operations.
 *
 *-------------------------------------------------------------------------
 */

#ifndef BULK_WRITE_H
#define BULK_WRITE_H

#include "io/bson_core.h"

/*
 * BulkWriteOperationType defines the types of operations supported in bulkWrite.
 */
typedef enum BulkWriteOperationType
{
	BULK_WRITE_UNKNOWN,
	BULK_WRITE_INSERT_ONE,
	BULK_WRITE_UPDATE_ONE,
	BULK_WRITE_UPDATE_MANY,
	BULK_WRITE_REPLACE_ONE,
	BULK_WRITE_DELETE_ONE,
	BULK_WRITE_DELETE_MANY
} BulkWriteOperationType;

/*
 * BulkWriteOperation represents a single operation in a bulk write request.
 */
typedef struct BulkWriteOperation
{
	BulkWriteOperationType type;
	bson_value_t operationSpec;
	int operationIndex;
} BulkWriteOperation;

/*
 * BulkWriteSpec describes a bulk write request.
 */
typedef struct BulkWriteSpec
{
	char *collectionName;
	List *operations;
	bool isOrdered;
	bool bypassDocumentValidation;
	bson_value_t variableSpec;
} BulkWriteSpec;


/*
 * BulkWriteResult contains the results of a bulk write operation.
 */
typedef struct BulkWriteResult
{
	double ok;
	uint64 insertedCount;
	uint64 matchedCount;
	uint64 modifiedCount;
	uint64 deletedCount;
	uint64 upsertedCount;
	List *upsertedIds;
	List *writeErrors;
	MemoryContext resultMemoryContext;
} BulkWriteResult;

#endif /* BULK_WRITE_H */
