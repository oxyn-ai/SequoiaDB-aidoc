
CREATE OR REPLACE FUNCTION documentdb_api.enable_pisa_integration(
    database_name text,
    collection_name text,
    compression_type int DEFAULT 1
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_enable_pisa_integration';

CREATE OR REPLACE FUNCTION documentdb_api.disable_pisa_integration(
    database_name text,
    collection_name text
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_disable_pisa_integration';

CREATE OR REPLACE FUNCTION documentdb_api.create_pisa_index(
    database_name text,
    collection_name text,
    compression_type int DEFAULT 1
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_create_pisa_index';

CREATE OR REPLACE FUNCTION documentdb_api.drop_pisa_index(
    database_name text,
    collection_name text
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_drop_pisa_index';

CREATE OR REPLACE FUNCTION documentdb_api.rebuild_pisa_index(
    database_name text,
    collection_name text
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_rebuild_pisa_index';

CREATE OR REPLACE FUNCTION documentdb_api.find_with_pisa_search(
    database_name text,
    command jsonb
) RETURNS jsonb
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_find_with_pisa_search';

CREATE OR REPLACE FUNCTION documentdb_api.pisa_index_status(
    database_name text DEFAULT NULL,
    collection_name text DEFAULT NULL
) RETURNS TABLE(
    database_name text,
    collection_name text,
    index_enabled boolean,
    last_sync_time timestamptz,
    pending_operations int,
    compression_type text,
    index_size_bytes bigint
)
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_pisa_index_status';

CREATE OR REPLACE FUNCTION documentdb_api.analyze_query_routing(
    query_json jsonb
) RETURNS TABLE(
    use_pisa boolean,
    use_hybrid boolean,
    query_type text,
    routing_reason text,
    estimated_cost numeric
)
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_analyze_query_routing';

CREATE OR REPLACE FUNCTION documentdb_api.execute_advanced_pisa_query(
    database_name text,
    collection_name text,
    query_terms jsonb,
    algorithm int DEFAULT 5,
    top_k int DEFAULT 10
) RETURNS TABLE(
    document_id text,
    score numeric,
    document jsonb,
    collection_id bigint
)
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_execute_advanced_pisa_query';

CREATE OR REPLACE FUNCTION documentdb_api.execute_pisa_wand_query(
    database_name text,
    collection_name text,
    query_terms jsonb,
    top_k int DEFAULT 10
) RETURNS TABLE(
    document_id text,
    score numeric,
    document jsonb
)
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_execute_pisa_wand_query';

CREATE OR REPLACE FUNCTION documentdb_api.execute_pisa_block_max_wand_query(
    database_name text,
    collection_name text,
    query_terms jsonb,
    top_k int DEFAULT 10
) RETURNS TABLE(
    document_id text,
    score numeric,
    document jsonb
)
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_execute_pisa_block_max_wand_query';

CREATE OR REPLACE FUNCTION documentdb_api.execute_pisa_maxscore_query(
    database_name text,
    collection_name text,
    query_terms jsonb,
    top_k int DEFAULT 10
) RETURNS TABLE(
    document_id text,
    score numeric,
    document jsonb
)
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_execute_pisa_maxscore_query';

CREATE OR REPLACE FUNCTION documentdb_api.analyze_pisa_query_plan(
    query_terms jsonb,
    top_k int DEFAULT 10
) RETURNS TABLE(
    selected_algorithm text,
    essential_terms jsonb,
    non_essential_terms jsonb,
    estimated_cost numeric,
    estimated_results int,
    use_block_max_optimization boolean,
    use_early_termination boolean
)
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_analyze_pisa_query_plan';

CREATE OR REPLACE FUNCTION documentdb_api.schedule_document_reordering(
    database_name text,
    collection_name text,
    priority int DEFAULT 1
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_schedule_document_reordering';

CREATE OR REPLACE FUNCTION documentdb_api.cancel_document_reordering(
    database_name text,
    collection_name text
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_cancel_document_reordering';

CREATE OR REPLACE FUNCTION documentdb_api.execute_recursive_graph_bisection(
    database_name text,
    collection_name text,
    depth int DEFAULT 8,
    cache_depth int DEFAULT 2
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_execute_recursive_graph_bisection';

CREATE OR REPLACE FUNCTION documentdb_api.get_reordering_stats(
    database_name text,
    collection_name text
) RETURNS TABLE(
    total_documents bigint,
    reordered_documents bigint,
    compression_ratio_before numeric,
    compression_ratio_after numeric,
    improvement_percentage numeric,
    last_reordering_time timestamptz,
    reordering_iterations int
)
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_get_reordering_stats';

CREATE OR REPLACE FUNCTION documentdb_api.get_all_reordering_tasks()
RETURNS TABLE(
    database_name text,
    collection_name text,
    scheduled_time timestamptz,
    started_time timestamptz,
    completed_time timestamptz,
    priority int,
    is_running boolean,
    is_completed boolean,
    compression_improvement numeric,
    documents_processed bigint
)
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_get_all_reordering_tasks';

CREATE OR REPLACE FUNCTION documentdb_api.create_pisa_text_index(
    database_name text,
    collection_name text,
    index_options jsonb DEFAULT '{}',
    compression_type int DEFAULT 1
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_create_pisa_text_index';

CREATE OR REPLACE FUNCTION documentdb_api.execute_pisa_text_query(
    database_name text,
    collection_name text,
    query_text text,
    limit_count int DEFAULT 10
) RETURNS TABLE(
    document_id text,
    score numeric,
    document jsonb,
    collection_id bigint
)
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_execute_pisa_text_query';

CREATE OR REPLACE FUNCTION documentdb_api.execute_hybrid_pisa_query(
    database_name text,
    collection_name text,
    text_query text DEFAULT NULL,
    filter_criteria jsonb DEFAULT '{}',
    sort_criteria jsonb DEFAULT '{}',
    limit_count int DEFAULT 10,
    offset_count int DEFAULT 0
) RETURNS TABLE(
    document_id text,
    score numeric,
    document jsonb,
    collection_id bigint
)
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_execute_hybrid_pisa_query';

CREATE OR REPLACE FUNCTION documentdb_api.optimize_pisa_text_index(
    database_name text,
    collection_name text
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_optimize_pisa_text_index';

CREATE OR REPLACE FUNCTION documentdb_api.export_collection_to_pisa_format(
    database_name text,
    collection_name text,
    output_path text
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_export_collection_to_pisa_format';

CREATE OR REPLACE FUNCTION documentdb_api.build_complete_pisa_index(
    database_name text,
    collection_name text,
    index_path text,
    compression_type int DEFAULT 1
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_build_complete_pisa_index';

CREATE OR REPLACE VIEW documentdb_api.pisa_configuration AS
SELECT 
    name,
    setting,
    unit,
    category,
    short_desc,
    extra_desc,
    context,
    vartype,
    source,
    min_val,
    max_val,
    enumvals,
    boot_val,
    reset_val,
    sourcefile,
    sourceline,
    pending_restart
FROM pg_settings 
WHERE name LIKE 'documentdb.pisa_%';

COMMENT ON VIEW documentdb_api.pisa_configuration IS 'PISA integration configuration parameters';

GRANT EXECUTE ON FUNCTION documentdb_api.enable_pisa_integration(text, text, int) TO documentdb_admin_role;
GRANT EXECUTE ON FUNCTION documentdb_api.disable_pisa_integration(text, text) TO documentdb_admin_role;
GRANT EXECUTE ON FUNCTION documentdb_api.create_pisa_index(text, text, int) TO documentdb_admin_role;
GRANT EXECUTE ON FUNCTION documentdb_api.drop_pisa_index(text, text) TO documentdb_admin_role;
GRANT EXECUTE ON FUNCTION documentdb_api.rebuild_pisa_index(text, text) TO documentdb_admin_role;
GRANT EXECUTE ON FUNCTION documentdb_api.find_with_pisa_search(text, jsonb) TO documentdb_readonly_role;
GRANT SELECT ON documentdb_api.pisa_index_status TO documentdb_readonly_role;
GRANT EXECUTE ON FUNCTION documentdb_api.analyze_query_routing(jsonb) TO documentdb_readonly_role;
GRANT SELECT ON documentdb_api.pisa_configuration TO documentdb_readonly_role;

GRANT EXECUTE ON FUNCTION documentdb_api.execute_advanced_pisa_query(text, text, jsonb, int, int) TO documentdb_readonly_role;
GRANT EXECUTE ON FUNCTION documentdb_api.execute_pisa_wand_query(text, text, jsonb, int) TO documentdb_readonly_role;
GRANT EXECUTE ON FUNCTION documentdb_api.execute_pisa_block_max_wand_query(text, text, jsonb, int) TO documentdb_readonly_role;
GRANT EXECUTE ON FUNCTION documentdb_api.execute_pisa_maxscore_query(text, text, jsonb, int) TO documentdb_readonly_role;
GRANT EXECUTE ON FUNCTION documentdb_api.analyze_pisa_query_plan(jsonb, int) TO documentdb_readonly_role;

GRANT EXECUTE ON FUNCTION documentdb_api.schedule_document_reordering(text, text, int) TO documentdb_admin_role;
GRANT EXECUTE ON FUNCTION documentdb_api.cancel_document_reordering(text, text) TO documentdb_admin_role;
GRANT EXECUTE ON FUNCTION documentdb_api.execute_recursive_graph_bisection(text, text, int, int) TO documentdb_admin_role;
GRANT EXECUTE ON FUNCTION documentdb_api.get_reordering_stats(text, text) TO documentdb_readonly_role;
GRANT EXECUTE ON FUNCTION documentdb_api.get_all_reordering_tasks() TO documentdb_readonly_role;

GRANT EXECUTE ON FUNCTION documentdb_api.create_pisa_text_index(text, text, jsonb, int) TO documentdb_admin_role;
GRANT EXECUTE ON FUNCTION documentdb_api.execute_pisa_text_query(text, text, text, int) TO documentdb_readonly_role;
GRANT EXECUTE ON FUNCTION documentdb_api.execute_hybrid_pisa_query(text, text, text, jsonb, jsonb, int, int) TO documentdb_readonly_role;
GRANT EXECUTE ON FUNCTION documentdb_api.optimize_pisa_text_index(text, text) TO documentdb_admin_role;

GRANT EXECUTE ON FUNCTION documentdb_api.export_collection_to_pisa_format(text, text, text) TO documentdb_admin_role;
GRANT EXECUTE ON FUNCTION documentdb_api.build_complete_pisa_index(text, text, text, int) TO documentdb_admin_role;

CREATE OR REPLACE FUNCTION documentdb_api.create_shard_mapping(
    database_name text,
    collection_name text,
    shard_count int,
    shard_strategy text DEFAULT 'hash'
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_create_shard_mapping';

CREATE OR REPLACE FUNCTION documentdb_api.drop_shard_mapping(
    database_name text,
    collection_name text
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_drop_shard_mapping';

CREATE OR REPLACE FUNCTION documentdb_api.get_shard_for_document(
    database_name text,
    collection_name text,
    document_id text
) RETURNS int
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_get_shard_for_document';

CREATE OR REPLACE FUNCTION documentdb_api.coordinate_sharded_query(
    database_name text,
    collection_name text,
    query jsonb
) RETURNS jsonb
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_coordinate_sharded_query';

CREATE OR REPLACE FUNCTION documentdb_api.balance_shards(
    database_name text,
    collection_name text
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_balance_shards';

CREATE OR REPLACE FUNCTION documentdb_api.get_shard_statistics(
    database_name text,
    collection_name text
) RETURNS jsonb
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_get_shard_statistics';

CREATE OR REPLACE FUNCTION documentdb_api.cache_pisa_query(
    cache_key text,
    result jsonb,
    ttl_seconds int DEFAULT 300
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_cache_pisa_query';

CREATE OR REPLACE FUNCTION documentdb_api.get_cached_pisa_query(
    cache_key text
) RETURNS jsonb
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_get_cached_pisa_query';

CREATE OR REPLACE FUNCTION documentdb_api.invalidate_pisa_cache(
    pattern text
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_invalidate_pisa_cache';

CREATE OR REPLACE FUNCTION documentdb_api.get_pisa_cache_stats()
RETURNS jsonb
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_get_pisa_cache_stats';

CREATE OR REPLACE FUNCTION documentdb_api.reset_pisa_cache()
RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_reset_pisa_cache';

CREATE OR REPLACE FUNCTION documentdb_api.record_pisa_metric(
    metric_type int,
    value float8,
    context text DEFAULT ''
) RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_record_pisa_metric';

CREATE OR REPLACE FUNCTION documentdb_api.get_pisa_metrics()
RETURNS jsonb
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_get_pisa_metrics';

CREATE OR REPLACE FUNCTION documentdb_api.get_pisa_performance_stats()
RETURNS jsonb
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_get_pisa_performance_stats';

CREATE OR REPLACE FUNCTION documentdb_api.optimize_pisa_performance()
RETURNS boolean
LANGUAGE C STRICT
AS 'MODULE_PATHNAME', 'documentdb_optimize_pisa_performance';

GRANT EXECUTE ON FUNCTION documentdb_api.create_shard_mapping(text, text, int, text) TO documentdb_admin_role;
GRANT EXECUTE ON FUNCTION documentdb_api.drop_shard_mapping(text, text) TO documentdb_admin_role;
GRANT EXECUTE ON FUNCTION documentdb_api.get_shard_for_document(text, text, text) TO documentdb_readonly_role;
GRANT EXECUTE ON FUNCTION documentdb_api.coordinate_sharded_query(text, text, jsonb) TO documentdb_readonly_role;
GRANT EXECUTE ON FUNCTION documentdb_api.balance_shards(text, text) TO documentdb_admin_role;
GRANT EXECUTE ON FUNCTION documentdb_api.get_shard_statistics(text, text) TO documentdb_readonly_role;

GRANT EXECUTE ON FUNCTION documentdb_api.cache_pisa_query(text, jsonb, int) TO documentdb_admin_role;
GRANT EXECUTE ON FUNCTION documentdb_api.get_cached_pisa_query(text) TO documentdb_readonly_role;
GRANT EXECUTE ON FUNCTION documentdb_api.invalidate_pisa_cache(text) TO documentdb_admin_role;
GRANT EXECUTE ON FUNCTION documentdb_api.get_pisa_cache_stats() TO documentdb_readonly_role;
GRANT EXECUTE ON FUNCTION documentdb_api.reset_pisa_cache() TO documentdb_admin_role;

GRANT EXECUTE ON FUNCTION documentdb_api.record_pisa_metric(int, float8, text) TO documentdb_admin_role;
GRANT EXECUTE ON FUNCTION documentdb_api.get_pisa_metrics() TO documentdb_readonly_role;
GRANT EXECUTE ON FUNCTION documentdb_api.get_pisa_performance_stats() TO documentdb_readonly_role;
GRANT EXECUTE ON FUNCTION documentdb_api.optimize_pisa_performance() TO documentdb_admin_role;
