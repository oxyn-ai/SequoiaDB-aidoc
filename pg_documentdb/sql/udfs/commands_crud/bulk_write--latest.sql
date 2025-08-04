/*
 * processes a MongoDB bulkWrite wire protocol command.
 */
CREATE OR REPLACE FUNCTION __API_SCHEMA_V2__.bulk_write(
    p_database_name text,
    p_bulk_write __CORE_SCHEMA_V2__.bson,
    p_bulk_operations __CORE_SCHEMA_V2__.bsonsequence default NULL,
    p_transaction_id text default NULL,
    p_result OUT __CORE_SCHEMA_V2__.bson,
    p_success OUT boolean)
 RETURNS record
 LANGUAGE C
AS 'MODULE_PATHNAME', $$command_bulk_write$$;
COMMENT ON FUNCTION __API_SCHEMA_V2__.bulk_write(text,__CORE_SCHEMA_V2__.bson,__CORE_SCHEMA_V2__.bsonsequence,text)
    IS 'executes multiple write operations in a single command for a mongo wire protocol command';
