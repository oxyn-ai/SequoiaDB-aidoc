
CREATE SCHEMA IF NOT EXISTS pisa_unit_test;
SET search_path TO documentdb_api, documentdb_core, pisa_unit_test, public;

SELECT 'Test 1: Data Bridge Component' as test_name;

SELECT documentdb_api.test_bson_to_pisa_conversion('{"title": "test document", "content": "sample text content"}');

SELECT documentdb_api.test_batch_bson_conversion('[{"_id": 1, "text": "first"}, {"_id": 2, "text": "second"}]');

SELECT 'Test 2: Index Synchronization Component' as test_name;

SELECT documentdb_api.test_index_sync_init();

SELECT documentdb_api.test_document_change_detection();

SELECT documentdb_api.test_incremental_index_update();

SELECT 'Test 3: Query Router Component' as test_name;

SELECT documentdb_api.test_query_type_detection('{"$text": {"$search": "test"}}');
SELECT documentdb_api.test_query_type_detection('{"field": "value"}');

SELECT documentdb_api.test_query_routing_decision('{"$text": {"$search": "machine learning"}}');

SELECT 'Test 4: PISA Export Component' as test_name;

SELECT documentdb_api.test_document_parsing('{"title": "Test", "content": "Sample content for testing"}');

SELECT documentdb_api.test_forward_index_creation();

SELECT 'Test 5: Advanced Query Algorithms Component' as test_name;

SELECT documentdb_api.test_wand_algorithm_init();

SELECT documentdb_api.test_block_max_wand_setup();

SELECT documentdb_api.test_maxscore_algorithm();

SELECT 'Test 6: Document Reordering Component' as test_name;

SELECT documentdb_api.test_graph_construction();

SELECT documentdb_api.test_recursive_bisection();

SELECT documentdb_api.test_reordering_effectiveness();

SELECT 'Test 7: Sharding Support Component' as test_name;

SELECT documentdb_api.test_shard_configuration();

SELECT documentdb_api.test_document_distribution();

SELECT documentdb_api.test_cross_shard_queries();

SELECT 'Test 8: Query Cache Component' as test_name;

SELECT documentdb_api.test_cache_key_generation('{"$text": {"$search": "test query"}}');

SELECT documentdb_api.test_cache_storage_retrieval();

SELECT documentdb_api.test_cache_eviction();

SELECT 'Test 9: Performance Monitor Component' as test_name;

SELECT documentdb_api.test_metric_recording();

SELECT documentdb_api.test_threshold_checking();

SELECT documentdb_api.test_alert_generation();

SELECT 'Test 10: Memory Optimization Component' as test_name;

SELECT documentdb_api.test_memory_usage_tracking();

SELECT documentdb_api.test_memory_optimization();

SELECT documentdb_api.test_garbage_collection();

SELECT 'PISA Unit Tests Completed Successfully' as final_result;
