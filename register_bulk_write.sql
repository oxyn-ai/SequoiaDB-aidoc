CREATE OR REPLACE FUNCTION documentdb_api.bulk_write(
    p_database_name text,
    p_bulk_write documentdb_core.bson,
    p_bulk_operations documentdb_core.bsonsequence default NULL,
    p_transaction_id text default NULL,
    p_result OUT documentdb_core.bson,
    p_success OUT boolean)
 RETURNS record
 LANGUAGE C
AS 'pg_documentdb', 'command_bulk_write';

COMMENT ON FUNCTION documentdb_api.bulk_write(text,documentdb_core.bson,documentdb_core.bsonsequence,text)
    IS 'executes multiple write operations in a single command for a mongo wire protocol command';
