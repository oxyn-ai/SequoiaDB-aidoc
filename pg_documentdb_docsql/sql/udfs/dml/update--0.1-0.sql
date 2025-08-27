CREATE OR REPLACE FUNCTION documentdb_docsql.update_table(
    database_name text,
    table_name text,
    where_conditions text,
    update_operations text
) RETURNS record
LANGUAGE plpgsql
AS $$
DECLARE
    update_spec documentdb_core.bson;
    filter_bson documentdb_core.bson;
    update_bson documentdb_core.bson;
    result record;
BEGIN
    filter_bson := where_conditions::documentdb_core.bson;
    update_bson := update_operations::documentdb_core.bson;
    
    SELECT documentdb_core.bson_build_object(
        'update', table_name,
        'updates', documentdb_core.bson_build_array(
            documentdb_core.bson_build_object(
                'q', filter_bson,
                'u', update_bson
            )
        )
    ) INTO update_spec;
    
    SELECT * FROM documentdb_api.update(database_name, update_spec) INTO result;
    RETURN result;
END;
$$;

COMMENT ON FUNCTION documentdb_docsql.update_table IS 
'SQL DML interface for updating documents in collections';
