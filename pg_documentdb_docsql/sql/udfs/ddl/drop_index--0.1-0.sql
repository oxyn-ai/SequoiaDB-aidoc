CREATE OR REPLACE FUNCTION documentdb_docsql.drop_index(
    database_name text,
    table_name text,
    index_name text
) RETURNS void
LANGUAGE plpgsql
AS $$
DECLARE
    drop_spec documentdb_core.bson;
BEGIN
    SELECT documentdb_core.bson_build_object(
        'dropIndexes', table_name,
        'index', index_name
    ) INTO drop_spec;
    
    CALL documentdb_api.drop_indexes(database_name, drop_spec);
END;
$$;

COMMENT ON FUNCTION documentdb_docsql.drop_index IS 
'SQL DDL interface for dropping indexes on collections';
