CREATE OR REPLACE FUNCTION documentdb_docsql.drop_table(
    database_name text,
    table_name text
) RETURNS boolean
LANGUAGE plpgsql
AS $$
BEGIN
    RETURN documentdb_api.drop_collection(database_name, table_name);
END;
$$;

COMMENT ON FUNCTION documentdb_docsql.drop_table IS 
'SQL DDL interface for dropping collections as tables';
