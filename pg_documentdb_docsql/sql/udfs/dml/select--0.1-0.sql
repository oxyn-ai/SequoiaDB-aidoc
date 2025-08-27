CREATE OR REPLACE FUNCTION documentdb_docsql.select_from(
    database_name text,
    table_name text,
    where_conditions text DEFAULT '{}',
    projection_fields text[] DEFAULT NULL
) RETURNS SETOF documentdb_core.bson
LANGUAGE plpgsql
AS $$
DECLARE
    find_spec documentdb_core.bson;
    projection_bson documentdb_core.bson;
    filter_bson documentdb_core.bson;
BEGIN
    filter_bson := where_conditions::documentdb_core.bson;
    
    IF projection_fields IS NOT NULL THEN
        SELECT documentdb_core.bson_build_object_from_array(projection_fields, 1) INTO projection_bson;
        SELECT documentdb_core.bson_build_object(
            'find', table_name,
            'filter', filter_bson,
            'projection', projection_bson
        ) INTO find_spec;
    ELSE
        SELECT documentdb_core.bson_build_object(
            'find', table_name,
            'filter', filter_bson
        ) INTO find_spec;
    END IF;
    
    RETURN QUERY SELECT document FROM documentdb_api_catalog.bson_aggregation_find(database_name, find_spec);
END;
$$;

COMMENT ON FUNCTION documentdb_docsql.select_from IS 
'SQL DML interface for querying documents from collections';
