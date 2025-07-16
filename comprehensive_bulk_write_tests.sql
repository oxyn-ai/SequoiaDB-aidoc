
\echo '=== DocumentDB bulkWrite 功能综合测试 ==='
\echo '=== Comprehensive DocumentDB bulkWrite Functionality Tests ==='

SET search_path TO documentdb_core,documentdb_api,documentdb_api_catalog,documentdb_api_internal;

SELECT documentdb_api.drop_collection('testdb', 'bulk_test_collection');
SELECT documentdb_api.create_collection('testdb', 'bulk_test_collection');

\echo ''
\echo '测试 1: insertOne 操作测试'
\echo 'Test 1: insertOne Operation Test'

BEGIN;
SELECT documentdb_api.bulk_write('testdb', '{ "bulkWrite": "bulk_test_collection", "ops": [
    { "insertOne": { "document": { "_id": 1, "name": "Alice", "age": 25, "department": "Engineering" } } }
] }') AS result;

SELECT COUNT(*) as inserted_count FROM documentdb_api.collection('testdb', 'bulk_test_collection');
SELECT document FROM documentdb_api.collection('testdb', 'bulk_test_collection') WHERE document @@ '{"_id": 1}';
ROLLBACK;

\echo ''
\echo '测试 2: 多个 insertOne 操作测试'
\echo 'Test 2: Multiple insertOne Operations Test'

BEGIN;
SELECT documentdb_api.bulk_write('testdb', '{ "bulkWrite": "bulk_test_collection", "ops": [
    { "insertOne": { "document": { "_id": 1, "name": "Alice", "age": 25 } } },
    { "insertOne": { "document": { "_id": 2, "name": "Bob", "age": 30 } } },
    { "insertOne": { "document": { "_id": 3, "name": "Charlie", "age": 35 } } }
] }') AS result;

SELECT COUNT(*) as total_documents FROM documentdb_api.collection('testdb', 'bulk_test_collection');
ROLLBACK;

\echo ''
\echo '测试 3: updateOne 操作测试'
\echo 'Test 3: updateOne Operation Test'

BEGIN;
SELECT documentdb_api.insert_one('testdb', 'bulk_test_collection', '{ "_id": 1, "name": "Alice", "age": 25, "status": "active" }');

SELECT documentdb_api.bulk_write('testdb', '{ "bulkWrite": "bulk_test_collection", "ops": [
    { "updateOne": { "filter": { "_id": 1 }, "update": { "$set": { "age": 26, "updated": true } } } }
] }') AS result;

SELECT document FROM documentdb_api.collection('testdb', 'bulk_test_collection') WHERE document @@ '{"_id": 1}';
ROLLBACK;

\echo ''
\echo '测试 4: updateMany 操作测试'
\echo 'Test 4: updateMany Operation Test'

BEGIN;
SELECT documentdb_api.insert('testdb', '{ "insert": "bulk_test_collection", "documents": [
    { "_id": 1, "category": "A", "value": 10 },
    { "_id": 2, "category": "A", "value": 20 },
    { "_id": 3, "category": "B", "value": 30 }
] }');

SELECT documentdb_api.bulk_write('testdb', '{ "bulkWrite": "bulk_test_collection", "ops": [
    { "updateMany": { "filter": { "category": "A" }, "update": { "$inc": { "value": 5 } } } }
] }') AS result;

SELECT document FROM documentdb_api.collection('testdb', 'bulk_test_collection') ORDER BY (document->>'_id')::int;
ROLLBACK;

\echo ''
\echo '测试 5: replaceOne 操作测试'
\echo 'Test 5: replaceOne Operation Test'

BEGIN;
SELECT documentdb_api.insert_one('testdb', 'bulk_test_collection', '{ "_id": 1, "name": "Alice", "age": 25, "city": "NYC", "status": "active" }');

SELECT documentdb_api.bulk_write('testdb', '{ "bulkWrite": "bulk_test_collection", "ops": [
    { "replaceOne": { "filter": { "_id": 1 }, "replacement": { "_id": 1, "name": "Alice Smith", "age": 26, "department": "Engineering" } } }
] }') AS result;

SELECT document FROM documentdb_api.collection('testdb', 'bulk_test_collection') WHERE document @@ '{"_id": 1}';
ROLLBACK;

\echo ''
\echo '测试 6: deleteOne 操作测试'
\echo 'Test 6: deleteOne Operation Test'

BEGIN;
SELECT documentdb_api.insert('testdb', '{ "insert": "bulk_test_collection", "documents": [
    { "_id": 1, "status": "active" },
    { "_id": 2, "status": "inactive" },
    { "_id": 3, "status": "inactive" }
] }');

SELECT documentdb_api.bulk_write('testdb', '{ "bulkWrite": "bulk_test_collection", "ops": [
    { "deleteOne": { "filter": { "status": "inactive" } } }
] }') AS result;

SELECT COUNT(*) as remaining_count FROM documentdb_api.collection('testdb', 'bulk_test_collection');
SELECT document FROM documentdb_api.collection('testdb', 'bulk_test_collection') ORDER BY (document->>'_id')::int;
ROLLBACK;

\echo ''
\echo '测试 7: deleteMany 操作测试'
\echo 'Test 7: deleteMany Operation Test'

BEGIN;
SELECT documentdb_api.insert('testdb', '{ "insert": "bulk_test_collection", "documents": [
    { "_id": 1, "status": "active" },
    { "_id": 2, "status": "inactive" },
    { "_id": 3, "status": "inactive" },
    { "_id": 4, "status": "pending" }
] }');

SELECT documentdb_api.bulk_write('testdb', '{ "bulkWrite": "bulk_test_collection", "ops": [
    { "deleteMany": { "filter": { "status": "inactive" } } }
] }') AS result;

SELECT COUNT(*) as remaining_count FROM documentdb_api.collection('testdb', 'bulk_test_collection');
SELECT document FROM documentdb_api.collection('testdb', 'bulk_test_collection') ORDER BY (document->>'_id')::int;
ROLLBACK;

\echo ''
\echo '测试 8: 混合操作测试'
\echo 'Test 8: Mixed Operations Test'

BEGIN;
SELECT documentdb_api.insert_one('testdb', 'bulk_test_collection', '{ "_id": 1, "name": "Alice", "status": "active" }');

SELECT documentdb_api.bulk_write('testdb', '{ "bulkWrite": "bulk_test_collection", "ops": [
    { "insertOne": { "document": { "_id": 2, "name": "Bob", "status": "new" } } },
    { "updateOne": { "filter": { "_id": 1 }, "update": { "$set": { "updated": true } } } },
    { "insertOne": { "document": { "_id": 3, "name": "Charlie", "status": "pending" } } },
    { "deleteOne": { "filter": { "status": "pending" } } }
] }') AS result;

SELECT COUNT(*) as final_count FROM documentdb_api.collection('testdb', 'bulk_test_collection');
SELECT document FROM documentdb_api.collection('testdb', 'bulk_test_collection') ORDER BY (document->>'_id')::int;
ROLLBACK;

\echo ''
\echo '测试 9: 有序执行模式测试（遇到错误时停止）'
\echo 'Test 9: Ordered Execution Mode Test (Stop on Error)'

BEGIN;
SELECT documentdb_api.insert_one('testdb', 'bulk_test_collection', '{ "_id": 1, "name": "Existing" }');

SELECT documentdb_api.bulk_write('testdb', '{ "bulkWrite": "bulk_test_collection", "ordered": true, "ops": [
    { "insertOne": { "document": { "_id": 2, "name": "Bob" } } },
    { "insertOne": { "document": { "_id": 1, "name": "Duplicate" } } },
    { "insertOne": { "document": { "_id": 3, "name": "Charlie" } } }
] }') AS result;

SELECT COUNT(*) as count_after_ordered_error FROM documentdb_api.collection('testdb', 'bulk_test_collection');
ROLLBACK;

\echo ''
\echo '测试 10: 无序执行模式测试（继续执行其他操作）'
\echo 'Test 10: Unordered Execution Mode Test (Continue Other Operations)'

BEGIN;
SELECT documentdb_api.insert_one('testdb', 'bulk_test_collection', '{ "_id": 1, "name": "Existing" }');

SELECT documentdb_api.bulk_write('testdb', '{ "bulkWrite": "bulk_test_collection", "ordered": false, "ops": [
    { "insertOne": { "document": { "_id": 2, "name": "Bob" } } },
    { "insertOne": { "document": { "_id": 1, "name": "Duplicate" } } },
    { "insertOne": { "document": { "_id": 3, "name": "Charlie" } } }
] }') AS result;

SELECT COUNT(*) as count_after_unordered_error FROM documentdb_api.collection('testdb', 'bulk_test_collection');
SELECT document FROM documentdb_api.collection('testdb', 'bulk_test_collection') ORDER BY (document->>'_id')::int;
ROLLBACK;

\echo ''
\echo '测试 11: upsert 操作测试'
\echo 'Test 11: Upsert Operation Test'

BEGIN;
SELECT documentdb_api.bulk_write('testdb', '{ "bulkWrite": "bulk_test_collection", "ops": [
    { "updateOne": { "filter": { "_id": 1 }, "update": { "$set": { "name": "Alice", "age": 25 } }, "upsert": true } },
    { "updateOne": { "filter": { "_id": 2 }, "update": { "$set": { "name": "Bob", "age": 30 } }, "upsert": true } }
] }') AS result;

SELECT COUNT(*) as upserted_count FROM documentdb_api.collection('testdb', 'bulk_test_collection');
SELECT document FROM documentdb_api.collection('testdb', 'bulk_test_collection') ORDER BY (document->>'_id')::int;
ROLLBACK;

\echo ''
\echo '测试 12: 空操作数组测试'
\echo 'Test 12: Empty Operations Array Test'

BEGIN;
SELECT documentdb_api.bulk_write('testdb', '{ "bulkWrite": "bulk_test_collection", "ops": [] }') AS result;
ROLLBACK;

\echo ''
\echo '测试 13: 错误处理测试'
\echo 'Test 13: Error Handling Test'

BEGIN;
SELECT documentdb_api.bulk_write('testdb', '{ "bulkWrite": "bulk_test_collection", "ops": [
    { "invalidOperation": { "document": { "_id": 1, "name": "Test" } } }
] }') AS result;
ROLLBACK;

BEGIN;
SELECT documentdb_api.bulk_write('testdb', '{ "bulkWrite": "bulk_test_collection", "ops": [
    { "insertOne": { "document": { "name": "No ID" } } }
] }') AS result;
ROLLBACK;

\echo ''
\echo '测试 14: 性能测试（批量插入）'
\echo 'Test 14: Performance Test (Bulk Insert)'

BEGIN;
\timing on
SELECT documentdb_api.bulk_write('testdb', '{ "bulkWrite": "bulk_test_collection", "ops": [
    { "insertOne": { "document": { "_id": 1, "value": 1 } } },
    { "insertOne": { "document": { "_id": 2, "value": 2 } } },
    { "insertOne": { "document": { "_id": 3, "value": 3 } } },
    { "insertOne": { "document": { "_id": 4, "value": 4 } } },
    { "insertOne": { "document": { "_id": 5, "value": 5 } } },
    { "insertOne": { "document": { "_id": 6, "value": 6 } } },
    { "insertOne": { "document": { "_id": 7, "value": 7 } } },
    { "insertOne": { "document": { "_id": 8, "value": 8 } } },
    { "insertOne": { "document": { "_id": 9, "value": 9 } } },
    { "insertOne": { "document": { "_id": 10, "value": 10 } } }
] }') AS result;
\timing off

SELECT COUNT(*) as performance_test_count FROM documentdb_api.collection('testdb', 'bulk_test_collection');
ROLLBACK;

\echo ''
\echo '测试 15: bypassDocumentValidation 参数测试'
\echo 'Test 15: bypassDocumentValidation Parameter Test'

BEGIN;
SELECT documentdb_api.bulk_write('testdb', '{ "bulkWrite": "bulk_test_collection", "bypassDocumentValidation": true, "ops": [
    { "insertOne": { "document": { "_id": 1, "name": "Test Bypass" } } }
] }') AS result;

SELECT COUNT(*) as bypass_test_count FROM documentdb_api.collection('testdb', 'bulk_test_collection');
ROLLBACK;

SELECT documentdb_api.drop_collection('testdb', 'bulk_test_collection');

\echo ''
\echo '=== 测试完成 ==='
\echo '=== Tests Completed ==='
