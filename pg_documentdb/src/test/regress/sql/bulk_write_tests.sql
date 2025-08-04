SET search_path TO documentdb_core,documentdb_api,documentdb_api_catalog,documentdb_api_internal;
SET citus.next_shard_id TO 663000;
SET documentdb.next_collection_id TO 6630;
SET documentdb.next_collection_index_id TO 6630;

CREATE SCHEMA bulk_write_test;

SELECT documentdb_api.create_collection('db', 'bulk_write_collection');

BEGIN;
SELECT documentdb_api.bulk_write('db', '{ "bulkWrite": "bulk_write_collection" }', 
    '[
        { "insertOne": { "document": { "_id": 1, "name": "Alice", "age": 25 } } },
        { "insertOne": { "document": { "_id": 2, "name": "Bob", "age": 30 } } },
        { "updateOne": { "filter": { "_id": 1 }, "update": { "$set": { "age": 26 } } } },
        { "deleteOne": { "filter": { "_id": 2 } } }
    ]'::bsonsequence);
SELECT COUNT(*) FROM documentdb_api.collection('db', 'bulk_write_collection');
SELECT document FROM documentdb_api.collection('db', 'bulk_write_collection') WHERE document @@ '{"_id": 1}';
ROLLBACK;

BEGIN;
SELECT documentdb_api.insert('db', '{ "insert": "bulk_write_collection", "documents": [
    { "_id": 1, "category": "A", "value": 10 },
    { "_id": 2, "category": "A", "value": 20 },
    { "_id": 3, "category": "B", "value": 30 },
    { "_id": 4, "category": "B", "value": 40 }
]}');

SELECT documentdb_api.bulk_write('db', '{ "bulkWrite": "bulk_write_collection" }', 
    '[
        { "updateMany": { "filter": { "category": "A" }, "update": { "$inc": { "value": 5 } } } },
        { "deleteMany": { "filter": { "category": "B" } } }
    ]'::bsonsequence);
SELECT COUNT(*) FROM documentdb_api.collection('db', 'bulk_write_collection');
SELECT document FROM documentdb_api.collection('db', 'bulk_write_collection') ORDER BY (document->>'_id')::int;
ROLLBACK;

BEGIN;
SELECT documentdb_api.insert_one('db', 'bulk_write_collection', '{ "_id": 1, "name": "Alice", "age": 25, "city": "NYC" }');

SELECT documentdb_api.bulk_write('db', '{ "bulkWrite": "bulk_write_collection" }', 
    '[
        { "replaceOne": { "filter": { "_id": 1 }, "replacement": { "_id": 1, "name": "Alice Smith", "age": 26 } } }
    ]'::bsonsequence);
SELECT document FROM documentdb_api.collection('db', 'bulk_write_collection') WHERE document @@ '{"_id": 1}';
ROLLBACK;

BEGIN;
SELECT documentdb_api.insert_one('db', 'bulk_write_collection', '{ "_id": 1, "name": "Alice" }');

SELECT documentdb_api.bulk_write('db', '{ "bulkWrite": "bulk_write_collection", "ordered": true }', 
    '[
        { "insertOne": { "document": { "_id": 2, "name": "Bob" } } },
        { "insertOne": { "document": { "_id": 1, "name": "Charlie" } } },
        { "insertOne": { "document": { "_id": 3, "name": "David" } } }
    ]'::bsonsequence);
ROLLBACK;

BEGIN;
SELECT documentdb_api.insert_one('db', 'bulk_write_collection', '{ "_id": 1, "name": "Alice" }');

SELECT documentdb_api.bulk_write('db', '{ "bulkWrite": "bulk_write_collection", "ordered": false }', 
    '[
        { "insertOne": { "document": { "_id": 2, "name": "Bob" } } },
        { "insertOne": { "document": { "_id": 1, "name": "Charlie" } } },
        { "insertOne": { "document": { "_id": 3, "name": "David" } } }
    ]'::bsonsequence);
SELECT COUNT(*) FROM documentdb_api.collection('db', 'bulk_write_collection');
ROLLBACK;

BEGIN;
SELECT documentdb_api.bulk_write('db', '{ "bulkWrite": "bulk_write_collection" }', 
    '[]'::bsonsequence);
ROLLBACK;

BEGIN;
CREATE FUNCTION bulk_write_test.generate_bulk_operations(numOps int)
RETURNS bsonsequence
SET search_path TO documentdb_core,documentdb_api_catalog, pg_catalog
AS $fn$
DECLARE
    operations bsonsequence;
BEGIN
    WITH ops AS (
        SELECT array_agg(FORMAT('{ "insertOne": { "document": { "_id": %s, "value": %s } } }', g, g * 10)::bson) AS docs
        FROM generate_series(1, numOps) g
    )
    SELECT docs::bsonsequence INTO operations FROM ops;
    RETURN operations;
END;
$fn$ LANGUAGE plpgsql;

SELECT documentdb_api.bulk_write('db', '{ "bulkWrite": "bulk_write_collection" }', 
    bulk_write_test.generate_bulk_operations(100));
SELECT COUNT(*) FROM documentdb_api.collection('db', 'bulk_write_collection');
ROLLBACK;

BEGIN;
SELECT documentdb_api.bulk_write('db', '{ "bulkWrite": "bulk_write_collection" }', 
    '[
        { "updateOne": { "filter": { "_id": 1 }, "update": { "$set": { "name": "Alice", "age": 25 } }, "upsert": true } },
        { "updateOne": { "filter": { "_id": 2 }, "update": { "$set": { "name": "Bob", "age": 30 } }, "upsert": true } },
        { "updateOne": { "filter": { "_id": 1 }, "update": { "$inc": { "age": 1 } } } }
    ]'::bsonsequence);
SELECT COUNT(*) FROM documentdb_api.collection('db', 'bulk_write_collection');
SELECT document FROM documentdb_api.collection('db', 'bulk_write_collection') ORDER BY (document->>'_id')::int;
ROLLBACK;

BEGIN;
SELECT documentdb_api.insert('db', '{ "insert": "bulk_write_collection", "documents": [
    { "_id": 1, "status": "active" },
    { "_id": 2, "status": "inactive" },
    { "_id": 3, "status": "active" }
]}');

WITH result AS (
    SELECT documentdb_api.bulk_write('db', '{ "bulkWrite": "bulk_write_collection" }', 
        '[
            { "insertOne": { "document": { "_id": 4, "status": "new" } } },
            { "updateMany": { "filter": { "status": "active" }, "update": { "$set": { "updated": true } } } },
            { "deleteOne": { "filter": { "status": "inactive" } } }
        ]'::bsonsequence) AS bulk_result
)
SELECT 
    bulk_result->>'ok' as ok,
    bulk_result->>'insertedCount' as insertedCount,
    bulk_result->>'matchedCount' as matchedCount,
    bulk_result->>'modifiedCount' as modifiedCount,
    bulk_result->>'deletedCount' as deletedCount,
    bulk_result->>'upsertedCount' as upsertedCount
FROM result;
ROLLBACK;

BEGIN;
SELECT documentdb_api.insert_one('db', 'bulk_write_collection', '{ "_id": 1, "name": "Alice" }');

WITH result AS (
    SELECT documentdb_api.bulk_write('db', '{ "bulkWrite": "bulk_write_collection", "ordered": false }', 
        '[
            { "insertOne": { "document": { "_id": 2, "name": "Bob" } } },
            { "insertOne": { "document": { "_id": 1, "name": "Duplicate" } } },
            { "insertOne": { "document": { "_id": 3, "name": "Charlie" } } }
        ]'::bsonsequence) AS bulk_result
)
SELECT 
    bulk_result->>'ok' as ok,
    bulk_result->>'insertedCount' as insertedCount,
    jsonb_array_length(bulk_result->'writeErrors') as error_count
FROM result;
ROLLBACK;

BEGIN;
SELECT documentdb_api.bulk_write('db', '{ "bulkWrite": "bulk_write_collection", "bypassDocumentValidation": true }', 
    '[
        { "insertOne": { "document": { "_id": 1, "name": "Alice" } } }
    ]'::bsonsequence);
SELECT COUNT(*) FROM documentdb_api.collection('db', 'bulk_write_collection');
ROLLBACK;

BEGIN;
SELECT documentdb_api.bulk_write('db', '{ "bulkWrite": "bulk_write_collection" }', 
    '[
        { "invalidOperation": { "document": { "_id": 1, "name": "Alice" } } }
    ]'::bsonsequence);
ROLLBACK;

BEGIN;
SELECT documentdb_api.bulk_write('db', '{ "bulkWrite": "nonexistent_collection" }', 
    '[
        { "insertOne": { "document": { "_id": 1, "name": "Alice" } } }
    ]'::bsonsequence);
ROLLBACK;

DROP SCHEMA bulk_write_test CASCADE;
