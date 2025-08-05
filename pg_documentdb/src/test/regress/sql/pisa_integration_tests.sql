
CREATE SCHEMA IF NOT EXISTS pisa_test;
SET search_path TO documentdb_api, documentdb_core, pisa_test, public;

SELECT 'Test 1: Basic PISA Integration Setup' as test_name;

SELECT documentdb_api.enable_pisa_integration();

SELECT documentdb_api.is_pisa_integration_enabled() as pisa_enabled;

SELECT 'Test 2: Collection Creation and Data Setup' as test_name;

SELECT documentdb_api.create_collection('pisa_test_db', 'articles');

SELECT documentdb_api.insert_one('pisa_test_db', 'articles', '{"_id": 1, "title": "Machine Learning Fundamentals", "content": "Machine learning is a subset of artificial intelligence that focuses on algorithms", "category": "technology", "tags": ["AI", "ML", "algorithms"]}');
SELECT documentdb_api.insert_one('pisa_test_db', 'articles', '{"_id": 2, "title": "Database Systems Overview", "content": "Database systems provide efficient storage and retrieval of structured data", "category": "database", "tags": ["SQL", "NoSQL", "storage"]}');
SELECT documentdb_api.insert_one('pisa_test_db', 'articles', '{"_id": 3, "title": "Information Retrieval Techniques", "content": "Information retrieval involves finding relevant documents from large collections", "category": "search", "tags": ["IR", "search", "indexing"]}');
SELECT documentdb_api.insert_one('pisa_test_db', 'articles', '{"_id": 4, "title": "Text Processing Methods", "content": "Text processing includes tokenization, stemming, and semantic analysis", "category": "nlp", "tags": ["NLP", "text", "processing"]}');
SELECT documentdb_api.insert_one('pisa_test_db', 'articles', '{"_id": 5, "title": "Search Engine Architecture", "content": "Search engines use inverted indexes and ranking algorithms for fast retrieval", "category": "search", "tags": ["search", "indexing", "ranking"]}');

SELECT 'Test 3: PISA Text Index Creation' as test_name;

SELECT documentdb_api.create_pisa_text_index('pisa_test_db', 'articles', '{"content": "text"}', '{"name": "content_pisa_idx"}');

SELECT documentdb_api.create_pisa_text_index('pisa_test_db', 'articles', '{"title": "text"}', '{"name": "title_pisa_idx"}');

SELECT documentdb_api.get_pisa_index_info('pisa_test_db', 'articles');

SELECT 'Test 4: Basic PISA Text Search' as test_name;

SELECT documentdb_api.find('pisa_test_db', 'articles', '{"$text": {"$search": "machine learning"}}');

SELECT documentdb_api.find('pisa_test_db', 'articles', '{"$text": {"$search": "database storage"}}');

SELECT documentdb_api.find('pisa_test_db', 'articles', '{"$text": {"$search": "\"information retrieval\""}}');

SELECT 'Test 5: Enhanced PISA Text Search' as test_name;

SELECT documentdb_api.execute_enhanced_pisa_text_search('pisa_test_db', 'articles', 'machine learning algorithms', '{"limit": 3, "include_scores": true}');

SELECT documentdb_api.execute_hybrid_pisa_query('pisa_test_db', 'articles', '{"text_query": "search engine", "filters": {"category": "search"}}');

SELECT 'Test 6: Advanced Query Algorithms' as test_name;

SELECT documentdb_api.execute_advanced_pisa_query('pisa_test_db', 'articles', '{"algorithm": "wand", "query": "database systems", "k": 3}');

SELECT documentdb_api.execute_advanced_pisa_query('pisa_test_db', 'articles', '{"algorithm": "block_max_wand", "query": "text processing", "k": 2}');

SELECT documentdb_api.execute_advanced_pisa_query('pisa_test_db', 'articles', '{"algorithm": "maxscore", "query": "information retrieval", "k": 3}');

SELECT documentdb_api.analyze_pisa_query_plan('pisa_test_db', 'articles', '{"algorithm": "wand", "query": "machine learning"}');

SELECT 'Test 7: Document Reordering and Optimization' as test_name;

SELECT documentdb_api.schedule_document_reordering('pisa_test_db', 'articles', '{"algorithm": "recursive_graph_bisection", "priority": "high"}');

SELECT documentdb_api.get_reordering_status('pisa_test_db', 'articles');

SELECT documentdb_api.get_reordering_stats('pisa_test_db', 'articles');

SELECT 'Test 8: Data Export and Format Conversion' as test_name;

SELECT documentdb_api.export_collection_to_pisa_format('pisa_test_db', 'articles', '{"output_path": "/tmp/pisa_export", "format": "binary"}');

SELECT documentdb_api.export_collection_to_pisa_format('pisa_test_db', 'articles', '{"output_path": "/tmp/pisa_incremental", "format": "binary", "incremental": true}');

SELECT 'Test 9: Query Caching' as test_name;

SELECT documentdb_api.enable_pisa_query_cache('pisa_test_db', 'articles');

SELECT documentdb_api.find('pisa_test_db', 'articles', '{"$text": {"$search": "machine learning"}}');

SELECT documentdb_api.find('pisa_test_db', 'articles', '{"$text": {"$search": "machine learning"}}');

SELECT documentdb_api.get_pisa_cache_stats('pisa_test_db', 'articles');

SELECT documentdb_api.clear_pisa_query_cache('pisa_test_db', 'articles');

SELECT 'Test 10: Performance Monitoring' as test_name;

SELECT documentdb_api.record_pisa_metric('query_latency', 45.5, 'test_query_1');
SELECT documentdb_api.record_pisa_metric('memory_usage', 1024000, 'index_build');
SELECT documentdb_api.record_pisa_metric('cache_hit_ratio', 0.85, 'cache_performance');

SELECT documentdb_api.set_pisa_metric_threshold('query_latency', 100.0, 200.0);
SELECT documentdb_api.set_pisa_metric_threshold('memory_usage', 2000000, 4000000);

SELECT documentdb_api.get_pisa_metrics();

SELECT documentdb_api.get_pisa_performance_stats();

SELECT 'Test 11: Sharding Support' as test_name;

SELECT documentdb_api.configure_pisa_sharding('pisa_test_db', 'articles', '{"shard_count": 2, "strategy": "hash"}');

SELECT documentdb_api.get_pisa_sharding_status('pisa_test_db', 'articles');

SELECT 'Test 12: Index Management and Maintenance' as test_name;

SELECT documentdb_api.rebuild_pisa_index('pisa_test_db', 'articles', 'content_pisa_idx');

SELECT documentdb_api.optimize_pisa_index('pisa_test_db', 'articles', 'content_pisa_idx');

SELECT documentdb_api.get_pisa_index_stats('pisa_test_db', 'articles', 'content_pisa_idx');

SELECT 'Test 13: Error Handling and Edge Cases' as test_name;

SELECT documentdb_api.create_pisa_text_index('pisa_test_db', 'nonexistent', '{"content": "text"}', '{"name": "test_idx"}');

SELECT documentdb_api.execute_advanced_pisa_query('pisa_test_db', 'articles', '{"algorithm": "invalid_algo", "query": "test"}');

SELECT documentdb_api.find('pisa_test_db', 'articles', '{"$text": {"$search": ""}}');

SELECT 'Test 14: Compatibility with Existing DocumentDB Features' as test_name;

SELECT documentdb_api.find('pisa_test_db', 'articles', '{"category": "technology"}');

SELECT documentdb_api.aggregate('pisa_test_db', 'articles', '[{"$match": {"category": "search"}}, {"$count": "total"}]');

SELECT documentdb_api.create_indexes('pisa_test_db', 'articles', '{"indexes": [{"key": {"category": 1}, "name": "category_idx"}]}');

SELECT documentdb_api.update_one('pisa_test_db', 'articles', '{"_id": 1}', '{"$set": {"updated": true}}');

SELECT 'Test 15: Performance Comparison' as test_name;

SELECT documentdb_api.disable_pisa_integration();

\timing on
SELECT documentdb_api.find('pisa_test_db', 'articles', '{"$text": {"$search": "machine learning"}}');
\timing off

SELECT documentdb_api.enable_pisa_integration();

\timing on
SELECT documentdb_api.find('pisa_test_db', 'articles', '{"$text": {"$search": "machine learning"}}');
\timing off

SELECT 'Test 16: Cleanup and Resource Management' as test_name;

SELECT documentdb_api.drop_pisa_index('pisa_test_db', 'articles', 'content_pisa_idx');
SELECT documentdb_api.drop_pisa_index('pisa_test_db', 'articles', 'title_pisa_idx');

SELECT documentdb_api.clear_all_pisa_caches();

SELECT documentdb_api.reset_pisa_performance_stats();

SELECT documentdb_api.drop('pisa_test_db', 'articles');

SELECT documentdb_api.disable_pisa_integration();

SELECT documentdb_api.is_pisa_integration_enabled() as pisa_disabled;

SELECT 'PISA Integration Test Suite Completed Successfully' as final_result;
