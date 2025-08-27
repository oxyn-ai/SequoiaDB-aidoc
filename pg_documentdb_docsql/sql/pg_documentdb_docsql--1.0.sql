CREATE SCHEMA IF NOT EXISTS documentdb_docsql;

CREATE FUNCTION documentdb_docsql.version()
RETURNS text
LANGUAGE C
AS 'MODULE_PATHNAME', 'documentdb_docsql_version';
