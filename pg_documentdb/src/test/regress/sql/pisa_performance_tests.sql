
CREATE SCHEMA IF NOT EXISTS pisa_perf_test;
SET search_path TO documentdb_api, documentdb_core, pisa_perf_test, public;

SELECT 'Test 1: Performance Test Setup' as test_name;

SELECT documentdb_api.create_collection('perf_test_db', 'large_articles');

DO $$
DECLARE
    i INTEGER;
BEGIN
    FOR i IN 1..1000 LOOP
        PERFORM documentdb_api.insert_one('perf_test_db', 'large_articles', 
            format('{"_id": %s, "title": "Article %s", "content": "This is a comprehensive article about topic %s with detailed information and analysis. The content includes various keywords and phrases that will be used for text search performance testing. Machine learning, artificial intelligence, database systems, information retrieval, and text processing are common themes.", "category": "category_%s", "tags": ["tag_%s", "performance", "test"]}', 
            i, i, i % 10, i % 5, i % 20));
    END LOOP;
END $$;

SELECT 'Test 2: Baseline Performance (Without PISA)' as test_name;

SELECT documentdb_api.disable_pisa_integration();

SELECT documentdb_api.create_indexes('perf_test_db', 'large_articles', '{"indexes": [{"key": {"content": "text"}, "name": "content_text_idx"}]}');

\timing on
SELECT COUNT(*) FROM (
    SELECT documentdb_api.find('perf_test_db', 'large_articles', '{"$text": {"$search": "machine learning"}}')
) AS baseline_search;
\timing off

\timing on
SELECT COUNT(*) FROM (
    SELECT documentdb_api.find('perf_test_db', 'large_articles', '{"$text": {"$search": "artificial intelligence database systems"}}')
) AS baseline_complex_search;
\timing off

SELECT 'Test 3: PISA Enhanced Performance' as test_name;

SELECT documentdb_api.enable_pisa_integration();

SELECT documentdb_api.create_pisa_text_index('perf_test_db', 'large_articles', '{"content": "text"}', '{"name": "content_pisa_idx", "compression": "varintgb"}');

SELECT pg_sleep(2);

\timing on
SELECT COUNT(*) FROM (
    SELECT documentdb_api.find('perf_test_db', 'large_articles', '{"$text": {"$search": "machine learning"}}')
) AS pisa_search;
\timing off

\timing on
SELECT COUNT(*) FROM (
    SELECT documentdb_api.find('perf_test_db', 'large_articles', '{"$text": {"$search": "artificial intelligence database systems"}}')
) AS pisa_complex_search;
\timing off

SELECT 'Test 4: Advanced Algorithm Performance' as test_name;

\timing on
SELECT documentdb_api.execute_advanced_pisa_query('perf_test_db', 'large_articles', '{"algorithm": "wand", "query": "machine learning artificial intelligence", "k": 10}');
\timing off

\timing on
SELECT documentdb_api.execute_advanced_pisa_query('perf_test_db', 'large_articles', '{"algorithm": "block_max_wand", "query": "database systems information retrieval", "k": 10}');
\timing off

\timing on
SELECT documentdb_api.execute_advanced_pisa_query('perf_test_db', 'large_articles', '{"algorithm": "maxscore", "query": "text processing natural language", "k": 10}');
\timing off

SELECT 'Test 5: Index Size Comparison' as test_name;

SELECT pg_size_pretty(pg_total_relation_size('documentdb_data.documents_' || collection_id || '_content_text_idx')) as standard_index_size
FROM documentdb_api_catalog.collections 
WHERE database_name = 'perf_test_db' AND collection_name = 'large_articles';

SELECT documentdb_api.get_pisa_index_stats('perf_test_db', 'large_articles', 'content_pisa_idx');

SELECT 'Test 6: Memory Usage Comparison' as test_name;

SELECT documentdb_api.disable_pisa_integration();
SELECT pg_sleep(1);
SELECT pg_size_pretty(pg_database_size(current_database())) as standard_memory_usage;

SELECT documentdb_api.enable_pisa_integration();
SELECT pg_sleep(1);
SELECT pg_size_pretty(pg_database_size(current_database())) as pisa_memory_usage;

SELECT 'Test 7: Throughput Testing' as test_name;

DO $$
DECLARE
    start_time TIMESTAMP;
    end_time TIMESTAMP;
    query_count INTEGER := 100;
    i INTEGER;
BEGIN
    start_time := clock_timestamp();
    
    FOR i IN 1..query_count LOOP
        PERFORM documentdb_api.find('perf_test_db', 'large_articles', 
            format('{"$text": {"$search": "topic %s"}}', i % 10));
    END LOOP;
    
    end_time := clock_timestamp();
    
    RAISE NOTICE 'Throughput: % queries in % seconds (% QPS)', 
        query_count, 
        EXTRACT(EPOCH FROM (end_time - start_time)),
        query_count / EXTRACT(EPOCH FROM (end_time - start_time));
END $$;

SELECT 'Test 8: Cache Performance Impact' as test_name;

SELECT documentdb_api.enable_pisa_query_cache('perf_test_db', 'large_articles');

\timing on
SELECT documentdb_api.find('perf_test_db', 'large_articles', '{"$text": {"$search": "performance testing"}}');
\timing off

\timing on
SELECT documentdb_api.find('perf_test_db', 'large_articles', '{"$text": {"$search": "performance testing"}}');
\timing off

SELECT documentdb_api.get_pisa_cache_stats('perf_test_db', 'large_articles');

SELECT 'Test 9: Document Reordering Performance Impact' as test_name;

\timing on
SELECT documentdb_api.execute_advanced_pisa_query('perf_test_db', 'large_articles', '{"algorithm": "wand", "query": "comprehensive analysis", "k": 20}');
\timing off

SELECT documentdb_api.schedule_document_reordering('perf_test_db', 'large_articles', '{"algorithm": "recursive_graph_bisection"}');

SELECT pg_sleep(3);

\timing on
SELECT documentdb_api.execute_advanced_pisa_query('perf_test_db', 'large_articles', '{"algorithm": "wand", "query": "comprehensive analysis", "k": 20}');
\timing off

SELECT documentdb_api.get_reordering_stats('perf_test_db', 'large_articles');

SELECT 'Test 10: Performance Summary Report' as test_name;

SELECT documentdb_api.generate_pisa_performance_report(
    NOW() - INTERVAL '1 hour',
    NOW()
);

SELECT documentdb_api.get_pisa_performance_stats();

SELECT documentdb_api.drop('perf_test_db', 'large_articles');
SELECT documentdb_api.disable_pisa_integration();

SELECT 'PISA Performance Tests Completed Successfully' as final_result;
