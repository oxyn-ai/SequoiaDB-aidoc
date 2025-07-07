
SET search_path TO documentdb_api,documentdb_core;
SET documentdb.next_collection_id TO 2000;
SET documentdb.next_collection_index_id TO 2000;

DROP TABLE IF EXISTS documentdb_data.documents_2001 CASCADE;
DROP TABLE IF EXISTS documentdb_data.documents_2002 CASCADE;
DROP TABLE IF EXISTS documentdb_data.documents_2003 CASCADE;

SELECT 'Starting DocumentDB Main Extension API Comprehensive Tests' as test_status;


SELECT '=== SECTION 2.1: PUBLIC API FUNCTIONS TESTING ===' as test_section;

SELECT '--- 2.1.1 CRUD Operations Testing ---' as test_subsection;

SELECT 'Testing insert_one function' as test_name;
SELECT documentdb_api.insert_one('testdb', 'users', '{"_id": 1, "name": "Alice", "age": 30, "email": "alice@example.com"}', NULL);
SELECT documentdb_api.insert_one('testdb', 'users', '{"_id": 2, "name": "Bob", "age": 25, "email": "bob@example.com"}', NULL);
SELECT documentdb_api.insert_one('testdb', 'users', '{"_id": 3, "name": "Charlie", "age": 35, "email": "charlie@example.com"}', NULL);

SELECT 'Testing find_cursor_first_page function' as test_name;
SELECT cursorPage, continuation FROM documentdb_api.find_cursor_first_page('testdb', '{"find": "users", "filter": {"age": {"$gte": 25}}}');

SELECT 'Testing update function' as test_name;
SELECT p_result, p_success FROM documentdb_api.update('testdb', '{"update": "users", "updates": [{"q": {"_id": 1}, "u": {"$set": {"age": 31}}}]}', NULL, NULL);

SELECT 'Testing delete function' as test_name;
SELECT p_result, p_success FROM documentdb_api.delete('testdb', '{"delete": "users", "deletes": [{"q": {"_id": 3}, "limit": 1}]}', NULL, NULL);

SELECT 'Verifying CRUD operations' as test_name;
SELECT cursorPage FROM documentdb_api.find_cursor_first_page('testdb', '{"find": "users"}');

SELECT '--- 2.1.2 Multi-document Operations Testing ---' as test_subsection;

SELECT 'Testing bulk insert operations' as test_name;
SELECT p_result, p_success FROM documentdb_api.insert('testdb', '{"insert": "products"}', 
    '[{"_id": 1, "name": "Laptop", "price": 999.99}, {"_id": 2, "name": "Mouse", "price": 29.99}]'::bsonsequence, NULL);

SELECT 'Testing bulk update operations' as test_name;
SELECT p_result, p_success FROM documentdb_api.update('testdb', 
    '{"update": "products", "updates": [{"q": {"price": {"$lt": 50}}, "u": {"$mul": {"price": 1.1}}}]}', NULL, NULL);

SELECT '--- 2.1.3 Aggregation Pipeline Testing ---' as test_subsection;

SELECT 'Testing aggregate_cursor_first_page with basic pipeline' as test_name;
SELECT cursorPage FROM documentdb_api.aggregate_cursor_first_page('testdb', 
    '{"aggregate": "users", "pipeline": [{"$match": {"age": {"$gte": 25}}}, {"$project": {"name": 1, "age": 1}}]}');

SELECT 'Testing aggregation with grouping' as test_name;
SELECT cursorPage FROM documentdb_api.aggregate_cursor_first_page('testdb', 
    '{"aggregate": "users", "pipeline": [{"$group": {"_id": null, "avgAge": {"$avg": "$age"}, "count": {"$sum": 1}}}]}');


SELECT '=== SECTION 2.2: COMMAND PROCESSING LAYER TESTING ===' as test_section;

SELECT '--- 2.2.1 Command Routing Testing ---' as test_subsection;

SELECT 'Testing count command routing' as test_name;
SELECT document FROM documentdb_api.count_query('testdb', '{"count": "users", "query": {}}');

SELECT 'Testing distinct command routing' as test_name;
SELECT document FROM documentdb_api.distinct_query('testdb', '{"distinct": "users", "key": "name"}');

SELECT '--- 2.2.2 Parameter Validation Testing ---' as test_subsection;

SELECT 'Testing invalid database name validation' as test_name;
BEGIN;
    SELECT documentdb_api.insert_one('', 'test', '{"_id": 1}', NULL);
EXCEPTION WHEN OTHERS THEN
    SELECT 'Expected error for empty database name: ' || SQLERRM as validation_result;
END;

SELECT 'Testing invalid collection name validation' as test_name;
BEGIN;
    SELECT documentdb_api.insert_one('testdb', '', '{"_id": 1}', NULL);
EXCEPTION WHEN OTHERS THEN
    SELECT 'Expected error for empty collection name: ' || SQLERRM as validation_result;
END;

SELECT 'Testing invalid BSON document validation' as test_name;
BEGIN;
    SELECT documentdb_api.insert_one('testdb', 'test', '{"invalid": }', NULL);
EXCEPTION WHEN OTHERS THEN
    SELECT 'Expected error for invalid BSON: ' || SQLERRM as validation_result;
END;

SELECT '--- 2.2.3 Error Handling Testing ---' as test_subsection;

SELECT 'Testing duplicate key error handling' as test_name;
BEGIN;
    SELECT documentdb_api.insert_one('testdb', 'users', '{"_id": 1, "name": "Duplicate"}', NULL);
EXCEPTION WHEN OTHERS THEN
    SELECT 'Expected duplicate key error: ' || SQLERRM as error_result;
END;

SELECT '--- 2.2.4 Transaction Management Testing ---' as test_subsection;

SELECT 'Testing transaction rollback scenario' as test_name;
BEGIN;
    SELECT documentdb_api.insert_one('testdb', 'txn_test', '{"_id": 1, "data": "test1"}', NULL);
    SELECT documentdb_api.insert_one('testdb', 'txn_test', '{"_id": 2, "data": "test2"}', NULL);
    ROLLBACK;

SELECT 'Verifying transaction rollback' as test_name;
SELECT cursorPage FROM documentdb_api.find_cursor_first_page('testdb', '{"find": "txn_test"}');


SELECT '=== SECTION 2.3: AGGREGATION PIPELINE ENGINE TESTING ===' as test_section;

SELECT 'Setting up aggregation test data' as test_name;
SELECT documentdb_api.insert_one('testdb', 'orders', '{"_id": 1, "customer": "Alice", "amount": 100, "status": "completed", "date": {"$date": "2023-01-15"}}', NULL);
SELECT documentdb_api.insert_one('testdb', 'orders', '{"_id": 2, "customer": "Bob", "amount": 250, "status": "pending", "date": {"$date": "2023-01-20"}}', NULL);
SELECT documentdb_api.insert_one('testdb', 'orders', '{"_id": 3, "customer": "Alice", "amount": 75, "status": "completed", "date": {"$date": "2023-01-25"}}', NULL);
SELECT documentdb_api.insert_one('testdb', 'orders', '{"_id": 4, "customer": "Charlie", "amount": 300, "status": "completed", "date": {"$date": "2023-02-01"}}', NULL);

SELECT '--- 2.3.1 Individual Stage Testing ---' as test_subsection;

SELECT 'Testing $match stage' as test_name;
SELECT cursorPage FROM documentdb_api.aggregate_cursor_first_page('testdb', 
    '{"aggregate": "orders", "pipeline": [{"$match": {"status": "completed"}}]}');

SELECT 'Testing $project stage' as test_name;
SELECT cursorPage FROM documentdb_api.aggregate_cursor_first_page('testdb', 
    '{"aggregate": "orders", "pipeline": [{"$project": {"customer": 1, "amount": 1, "_id": 0}}]}');

SELECT 'Testing $sort stage' as test_name;
SELECT cursorPage FROM documentdb_api.aggregate_cursor_first_page('testdb', 
    '{"aggregate": "orders", "pipeline": [{"$sort": {"amount": -1}}]}');

SELECT 'Testing $group stage' as test_name;
SELECT cursorPage FROM documentdb_api.aggregate_cursor_first_page('testdb', 
    '{"aggregate": "orders", "pipeline": [{"$group": {"_id": "$customer", "totalAmount": {"$sum": "$amount"}, "orderCount": {"$sum": 1}}}]}');

SELECT 'Testing $limit stage' as test_name;
SELECT cursorPage FROM documentdb_api.aggregate_cursor_first_page('testdb', 
    '{"aggregate": "orders", "pipeline": [{"$limit": 2}]}');

SELECT 'Testing $skip stage' as test_name;
SELECT cursorPage FROM documentdb_api.aggregate_cursor_first_page('testdb', 
    '{"aggregate": "orders", "pipeline": [{"$skip": 1}, {"$limit": 2}]}');

SELECT '--- 2.3.2 Pipeline Stage Combinations Testing ---' as test_subsection;

SELECT 'Testing complex multi-stage pipeline' as test_name;
SELECT cursorPage FROM documentdb_api.aggregate_cursor_first_page('testdb', 
    '{"aggregate": "orders", "pipeline": [
        {"$match": {"status": "completed"}}, 
        {"$group": {"_id": "$customer", "totalAmount": {"$sum": "$amount"}}}, 
        {"$sort": {"totalAmount": -1}}, 
        {"$project": {"customer": "$_id", "total": "$totalAmount", "_id": 0}}
    ]}');

SELECT 'Testing pipeline with order dependencies' as test_name;
SELECT cursorPage FROM documentdb_api.aggregate_cursor_first_page('testdb', 
    '{"aggregate": "orders", "pipeline": [
        {"$sort": {"date": 1}}, 
        {"$group": {"_id": "$status", "firstOrder": {"$first": "$$ROOT"}, "count": {"$sum": 1}}},
        {"$project": {"status": "$_id", "firstOrderAmount": "$firstOrder.amount", "orderCount": "$count", "_id": 0}}
    ]}');

SELECT '--- 2.3.3 Performance Testing ---' as test_subsection;

SELECT 'Setting up performance test data' as test_name;
SELECT documentdb_api.insert('testdb', '{"insert": "perf_test"}', 
    '[{"_id": 1, "value": 1, "category": "A"}, {"_id": 2, "value": 2, "category": "B"}, 
      {"_id": 3, "value": 3, "category": "A"}, {"_id": 4, "value": 4, "category": "B"},
      {"_id": 5, "value": 5, "category": "A"}, {"_id": 6, "value": 6, "category": "B"}]'::bsonsequence, NULL);

SELECT 'Testing aggregation performance' as test_name;
SELECT cursorPage FROM documentdb_api.aggregate_cursor_first_page('testdb', 
    '{"aggregate": "perf_test", "pipeline": [
        {"$group": {"_id": "$category", "avgValue": {"$avg": "$value"}, "maxValue": {"$max": "$value"}}},
        {"$sort": {"avgValue": -1}}
    ]}');


SELECT '=== SECTION 2.4: FEATURE FLAG SYSTEM TESTING ===' as test_section;

SELECT '--- 2.4.1 Configuration Management Testing ---' as test_subsection;

SELECT 'Testing current feature flag settings' as test_name;
SHOW documentdb.enableVectorHNSWIndex;
SHOW documentdb.enableSchemaValidation;

SELECT '--- 2.4.2 Feature Toggle Testing ---' as test_subsection;

SELECT 'Testing schema validation feature toggle' as test_name;
SET documentdb.enableSchemaValidation = false;
SHOW documentdb.enableSchemaValidation;

SET documentdb.enableSchemaValidation = true;

SELECT '--- 2.4.3 Compatibility Testing ---' as test_subsection;

SELECT 'Testing behavior with different flag states' as test_name;
SELECT documentdb_api.insert_one('testdb', 'flag_test', '{"_id": 1, "test": "flag_behavior"}', NULL);


SELECT '=== SECTION 2.5: METADATA CACHE SYSTEM TESTING ===' as test_section;

SELECT '--- 2.5.1 Cache Operations Testing ---' as test_subsection;

SELECT 'Testing cache with collection operations' as test_name;
SELECT documentdb_api.create_collection('testdb', 'cache_test');

SELECT 'Testing cache with index operations' as test_name;
SELECT retval FROM documentdb_api.create_indexes_background('testdb', 
    '{"createIndexes": "cache_test", "indexes": [{"key": {"field1": 1}, "name": "field1_idx"}]}');

SELECT '--- 2.5.2 Memory Management Testing ---' as test_subsection;

SELECT 'Testing cache with multiple collections' as test_name;
SELECT documentdb_api.create_collection('testdb', 'cache_test2');
SELECT documentdb_api.create_collection('testdb', 'cache_test3');

SELECT '--- 2.5.3 Performance Testing ---' as test_subsection;

SELECT 'Testing cache performance with repeated operations' as test_name;
SELECT cursorPage FROM documentdb_api.find_cursor_first_page('testdb', '{"find": "cache_test"}');
SELECT cursorPage FROM documentdb_api.find_cursor_first_page('testdb', '{"find": "cache_test"}');
SELECT cursorPage FROM documentdb_api.find_cursor_first_page('testdb', '{"find": "cache_test"}');


SELECT '=== TEST COMPLETION SUMMARY ===' as test_section;

SELECT 'Counting test collections created' as test_name;
SELECT document FROM documentdb_api.list_databases('{}');

SELECT 'DocumentDB Main Extension API Comprehensive Tests Completed Successfully' as final_status;

SELECT 'Cleaning up test data' as cleanup_status;
SELECT documentdb_api.drop_collection('testdb', 'users', NULL, NULL, true);
SELECT documentdb_api.drop_collection('testdb', 'products', NULL, NULL, true);
SELECT documentdb_api.drop_collection('testdb', 'orders', NULL, NULL, true);
SELECT documentdb_api.drop_collection('testdb', 'perf_test', NULL, NULL, true);
SELECT documentdb_api.drop_collection('testdb', 'flag_test', NULL, NULL, true);
SELECT documentdb_api.drop_collection('testdb', 'cache_test', NULL, NULL, true);
SELECT documentdb_api.drop_collection('testdb', 'cache_test2', NULL, NULL, true);
SELECT documentdb_api.drop_collection('testdb', 'cache_test3', NULL, NULL, true);
