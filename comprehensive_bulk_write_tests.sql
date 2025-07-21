
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
\echo '测试 16: 大批量操作测试 (1000个操作)'
\echo 'Test 16: Large Batch Operations Test (1000 operations)'

CREATE OR REPLACE FUNCTION test_large_bulk_operation()
RETURNS jsonb AS $$
DECLARE
    ops_array jsonb := '[]'::jsonb;
    single_op jsonb;
    i int;
    result jsonb;
BEGIN
    FOR i IN 1..1000 LOOP
        single_op := jsonb_build_object(
            'insertOne', jsonb_build_object(
                'document', jsonb_build_object(
                    '_id', i,
                    'data', repeat('x', 100),
                    'batch_id', 'large_test',
                    'index', i
                )
            )
        );
        ops_array := ops_array || single_op;
    END LOOP;
    
    SELECT documentdb_api.bulk_write('testdb', 
        jsonb_build_object(
            'bulkWrite', 'bulk_test_collection', 
            'ops', ops_array
        )
    ) INTO result;
    
    RETURN result;
END;
$$ LANGUAGE plpgsql;

BEGIN;
SELECT documentdb_api.create_collection('testdb', 'bulk_test_collection');
\timing on
SELECT test_large_bulk_operation() AS large_batch_result;
\timing off
SELECT COUNT(*) as large_batch_count FROM documentdb_api.collection('testdb', 'bulk_test_collection');
ROLLBACK;

\echo ''
\echo '测试 17: 混合大批量操作测试'
\echo 'Test 17: Mixed Large Batch Operations Test'

CREATE OR REPLACE FUNCTION test_mixed_large_bulk_operation()
RETURNS jsonb AS $$
DECLARE
    ops_array jsonb := '[]'::jsonb;
    single_op jsonb;
    i int;
    result jsonb;
BEGIN
    FOR i IN 1..300 LOOP
        single_op := jsonb_build_object(
            'insertOne', jsonb_build_object(
                'document', jsonb_build_object(
                    '_id', i,
                    'type', 'insert',
                    'value', i * 10
                )
            )
        );
        ops_array := ops_array || single_op;
    END LOOP;
    
    FOR i IN 1..200 LOOP
        single_op := jsonb_build_object(
            'updateMany', jsonb_build_object(
                'filter', jsonb_build_object('type', 'insert'),
                'update', jsonb_build_object('$inc', jsonb_build_object('value', 1))
            )
        );
        ops_array := ops_array || single_op;
    END LOOP;
    
    FOR i IN 250..349 LOOP
        single_op := jsonb_build_object(
            'deleteOne', jsonb_build_object(
                'filter', jsonb_build_object('_id', i)
            )
        );
        ops_array := ops_array || single_op;
    END LOOP;
    
    SELECT documentdb_api.bulk_write('testdb', 
        jsonb_build_object(
            'bulkWrite', 'bulk_test_collection', 
            'ops', ops_array
        )
    ) INTO result;
    
    RETURN result;
END;
$$ LANGUAGE plpgsql;

BEGIN;
SELECT documentdb_api.create_collection('testdb', 'bulk_test_collection');
\timing on
SELECT test_mixed_large_bulk_operation() AS mixed_large_batch_result;
\timing off
SELECT COUNT(*) as mixed_batch_final_count FROM documentdb_api.collection('testdb', 'bulk_test_collection');
ROLLBACK;

\echo ''
\echo '测试 18: 边界条件测试'
\echo 'Test 18: Edge Cases Test'

BEGIN;
SELECT documentdb_api.create_collection('testdb', 'bulk_test_collection');

SELECT documentdb_api.bulk_write('testdb', jsonb_build_object(
    'bulkWrite', 'bulk_test_collection',
    'ops', jsonb_build_array(
        jsonb_build_object(
            'insertOne', jsonb_build_object(
                'document', jsonb_build_object(
                    '_id', 1,
                    'large_field', repeat('A', 10000),
                    'metadata', jsonb_build_object(
                        'created', now()::text,
                        'size', 'large'
                    )
                )
            )
        )
    )
)) AS large_document_result;

SELECT documentdb_api.bulk_write('testdb', jsonb_build_object(
    'bulkWrite', 'bulk_test_collection',
    'ops', jsonb_build_array(
        jsonb_build_object(
            'insertOne', jsonb_build_object(
                'document', jsonb_build_object(
                    '_id', 2,
                    'level1', jsonb_build_object(
                        'level2', jsonb_build_object(
                            'level3', jsonb_build_object(
                                'level4', jsonb_build_object(
                                    'level5', jsonb_build_object(
                                        'deep_value', 'nested_data'
                                    )
                                )
                            )
                        )
                    )
                )
            )
        )
    )
)) AS nested_document_result;

SELECT COUNT(*) as edge_case_count FROM documentdb_api.collection('testdb', 'bulk_test_collection');
ROLLBACK;

\echo ''
\echo '测试 19: 错误恢复和事务测试'
\echo 'Test 19: Error Recovery and Transaction Test'

BEGIN;
SELECT documentdb_api.create_collection('testdb', 'bulk_test_collection');
SELECT documentdb_api.insert_one('testdb', 'bulk_test_collection', '{ "_id": 1, "name": "Existing" }');

SELECT documentdb_api.bulk_write('testdb', jsonb_build_object(
    'bulkWrite', 'bulk_test_collection',
    'ordered', true,
    'ops', jsonb_build_array(
        jsonb_build_object('insertOne', jsonb_build_object('document', jsonb_build_object('_id', 2, 'name', 'Valid1'))),
        jsonb_build_object('insertOne', jsonb_build_object('document', jsonb_build_object('_id', 3, 'name', 'Valid2'))),
        jsonb_build_object('insertOne', jsonb_build_object('document', jsonb_build_object('_id', 1, 'name', 'Duplicate'))),
        jsonb_build_object('insertOne', jsonb_build_object('document', jsonb_build_object('_id', 4, 'name', 'ShouldNotInsert')))
    )
)) AS ordered_error_recovery_result;

SELECT COUNT(*) as count_after_ordered_error FROM documentdb_api.collection('testdb', 'bulk_test_collection');

SELECT documentdb_api.bulk_write('testdb', jsonb_build_object(
    'bulkWrite', 'bulk_test_collection',
    'ordered', false,
    'ops', jsonb_build_array(
        jsonb_build_object('insertOne', jsonb_build_object('document', jsonb_build_object('_id', 5, 'name', 'Valid3'))),
        jsonb_build_object('insertOne', jsonb_build_object('document', jsonb_build_object('_id', 1, 'name', 'Duplicate2'))),
        jsonb_build_object('insertOne', jsonb_build_object('document', jsonb_build_object('_id', 6, 'name', 'Valid4')))
    )
)) AS unordered_error_recovery_result;

SELECT COUNT(*) as count_after_unordered_error FROM documentdb_api.collection('testdb', 'bulk_test_collection');
ROLLBACK;

\echo ''
\echo '测试 20: 复杂updateMany和deleteMany测试'
\echo 'Test 20: Complex updateMany and deleteMany Test'

BEGIN;
SELECT documentdb_api.create_collection('testdb', 'bulk_test_collection');

SELECT documentdb_api.insert('testdb', jsonb_build_object(
    'insert', 'bulk_test_collection',
    'documents', jsonb_build_array(
        jsonb_build_object('_id', 1, 'category', 'A', 'status', 'active', 'score', 10),
        jsonb_build_object('_id', 2, 'category', 'A', 'status', 'active', 'score', 20),
        jsonb_build_object('_id', 3, 'category', 'A', 'status', 'inactive', 'score', 15),
        jsonb_build_object('_id', 4, 'category', 'B', 'status', 'active', 'score', 25),
        jsonb_build_object('_id', 5, 'category', 'B', 'status', 'active', 'score', 30),
        jsonb_build_object('_id', 6, 'category', 'B', 'status', 'inactive', 'score', 5),
        jsonb_build_object('_id', 7, 'category', 'C', 'status', 'pending', 'score', 12)
    )
));

SELECT documentdb_api.bulk_write('testdb', jsonb_build_object(
    'bulkWrite', 'bulk_test_collection',
    'ops', jsonb_build_array(
        jsonb_build_object(
            'updateMany', jsonb_build_object(
                'filter', jsonb_build_object('category', 'A'),
                'update', jsonb_build_object(
                    '$inc', jsonb_build_object('score', 5),
                    '$set', jsonb_build_object('updated', true)
                )
            )
        ),
        jsonb_build_object(
            'updateMany', jsonb_build_object(
                'filter', jsonb_build_object('status', 'active'),
                'update', jsonb_build_object('$set', jsonb_build_object('last_active', 'today'))
            )
        ),
        jsonb_build_object(
            'deleteMany', jsonb_build_object(
                'filter', jsonb_build_object('status', 'inactive')
            )
        ),
        jsonb_build_object(
            'deleteMany', jsonb_build_object(
                'filter', jsonb_build_object('score', jsonb_build_object('$lt', 15))
            )
        )
    )
)) AS complex_operations_result;

SELECT COUNT(*) as final_document_count FROM documentdb_api.collection('testdb', 'bulk_test_collection');
SELECT document FROM documentdb_api.collection('testdb', 'bulk_test_collection') ORDER BY (document->>'_id')::int;
ROLLBACK;

DROP FUNCTION IF EXISTS test_large_bulk_operation();
DROP FUNCTION IF EXISTS test_mixed_large_bulk_operation();

SELECT documentdb_api.drop_collection('testdb', 'bulk_test_collection');

\echo ''
\echo '=== 增强测试完成 ==='
\echo '=== Enhanced Tests Completed ==='
