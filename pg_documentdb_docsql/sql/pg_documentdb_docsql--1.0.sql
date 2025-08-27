CREATE SCHEMA IF NOT EXISTS documentdb_docsql;

CREATE FUNCTION documentdb_docsql.version()
RETURNS text
LANGUAGE C
AS 'MODULE_PATHNAME', 'documentdb_docsql_version';

CREATE OR REPLACE FUNCTION documentdb_docsql.create_database(dbname text)
RETURNS void
LANGUAGE SQL
AS $$
SELECT documentdb_api.create_database(dbname);
$$;

CREATE OR REPLACE FUNCTION documentdb_docsql.drop_database(dbname text)
RETURNS void
LANGUAGE SQL
AS $$
SELECT documentdb_api.drop_database(dbname);
$$;

CREATE OR REPLACE FUNCTION documentdb_docsql.create_collection(dbname text, collname text)
RETURNS void
LANGUAGE plpgsql
AS $func$
DECLARE
  schemaname text := format('docsql_%s', dbname);
  viewname text := quote_ident(collname);
  fqview text;
BEGIN
  PERFORM documentdb_api.create_database(dbname);
  EXECUTE format('CREATE SCHEMA IF NOT EXISTS %I', schemaname);
  PERFORM documentdb_api.create_collection(dbname, collname);
  fqview := format('%I.%s', schemaname, viewname);
  EXECUTE format(
    'CREATE OR REPLACE VIEW %s AS SELECT document FROM documentdb_api.collection(%L,%L)',
    fqview, dbname, collname);

  EXECUTE format($trg$
    CREATE OR REPLACE FUNCTION %I.%I_insert_trg()
    RETURNS trigger
    LANGUAGE plpgsql
    AS $b$
    BEGIN
      PERFORM documentdb_api.insert_one(%L, %L, NEW.document::text);
      RETURN NEW;
    END
    $b$;$trg$, schemaname, collname || '_docsql');

  EXECUTE format(
    'DROP TRIGGER IF EXISTS %I ON %s',
    collname || '_docsql_ins', fqview);

  EXECUTE format(
    'CREATE TRIGGER %I INSTEAD OF INSERT ON %s FOR EACH ROW EXECUTE FUNCTION %I.%I_insert_trg()',
    collname || '_docsql_ins', fqview, schemaname, collname || '_docsql');
END;
$func$;

CREATE OR REPLACE FUNCTION documentdb_docsql.drop_collection(dbname text, collname text)
RETURNS void
LANGUAGE plpgsql
AS $func$
DECLARE
  schemaname text := format('docsql_%s', dbname);
  fqview text := format('%I.%I', schemaname, collname);
BEGIN
  EXECUTE format('DROP VIEW IF EXISTS %s CASCADE', fqview);
  PERFORM documentdb_api.drop_collection(dbname, collname);
END;
$func$;
