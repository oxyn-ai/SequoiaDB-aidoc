CREATE OR REPLACE FUNCTION documentdb_docsql.delete_from(
    database_name text,
    table_name text,
    where_conditions text
) RETURNS record
LANGUAGE plpgsql
AS $$
DECLARE
    delete_spec documentdb_core.bson;
    filter_bson documentdb_core.bson;
    result record;
BEGIN
    filter_bson := where_conditions::documentdb_core.bson;
    
    SELECT documentdb_core.bson_build_object(
        'delete', table_name,
        'deletes', documentdb_core.bson_build_array(
            documentdb_core.bson_build_object(
                'q', filter_bson,
                'limit', 0
            )
        )
    ) INTO delete_spec;
    
    SELECT * FROM documentdb_api.delete(database_name, delete_spec) INTO result;
    RETURN result;
END;
$$;

COMMENT ON FUNCTION documentdb_docsql.delete_from IS 
'SQL DML interface for deleting documents from collections';
