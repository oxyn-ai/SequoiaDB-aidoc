CREATE OR REPLACE FUNCTION documentdb_docsql.create_index(
    database_name text,
    table_name text,
    index_name text,
    index_fields text[]
) RETURNS record
LANGUAGE plpgsql
AS $$
DECLARE
    index_spec documentdb_core.bson;
    key_spec documentdb_core.bson;
    result record;
BEGIN
    SELECT documentdb_core.bson_build_object_from_array(index_fields, 1) INTO key_spec;
    
    SELECT documentdb_core.bson_build_object(
        'createIndexes', table_name,
        'indexes', documentdb_core.bson_build_array(
            documentdb_core.bson_build_object(
                'key', key_spec,
                'name', index_name
            )
        )
    ) INTO index_spec;
    
    SELECT * FROM documentdb_api.create_indexes_background(database_name, index_spec) INTO result;
    RETURN result;
END;
$$;

COMMENT ON FUNCTION documentdb_docsql.create_index IS 
'SQL DDL interface for creating indexes on collections';
