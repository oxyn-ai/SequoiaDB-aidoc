CREATE OR REPLACE FUNCTION documentdb_docsql.get_version()
RETURNS text
LANGUAGE sql
IMMUTABLE
AS $$
    SELECT '0.1-0'::text;
$$;

COMMENT ON FUNCTION documentdb_docsql.get_version IS 
'Returns the version of the documentdb_docsql extension';
