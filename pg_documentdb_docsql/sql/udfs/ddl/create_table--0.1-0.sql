CREATE OR REPLACE FUNCTION documentdb_docsql.create_table(
    database_name text,
    table_name text
) RETURNS boolean
LANGUAGE plpgsql
AS $$
BEGIN
    RETURN documentdb_api.create_collection(database_name, table_name);
END;
$$;

COMMENT ON FUNCTION documentdb_docsql.create_table IS 
'SQL DDL interface for creating collections as tables';
