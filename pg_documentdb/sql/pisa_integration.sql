
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
