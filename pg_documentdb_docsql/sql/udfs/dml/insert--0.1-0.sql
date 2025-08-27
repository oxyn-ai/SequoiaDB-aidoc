CREATE OR REPLACE FUNCTION documentdb_docsql.insert_into(
    database_name text,
    table_name text,
    column_names text[],
    column_values text[]
) RETURNS documentdb_core.bson
LANGUAGE plpgsql
AS $$
DECLARE
    bson_doc documentdb_core.bson;
    i integer;
    doc_text text := '{';
BEGIN
    IF array_length(column_names, 1) != array_length(column_values, 1) THEN
        RAISE EXCEPTION 'Column names and values arrays must have the same length';
    END IF;
    
    FOR i IN 1..array_length(column_names, 1) LOOP
        IF i > 1 THEN
            doc_text := doc_text || ', ';
        END IF;
        doc_text := doc_text || '"' || column_names[i] || '": "' || column_values[i] || '"';
    END LOOP;
    
    doc_text := doc_text || '}';
    bson_doc := doc_text::documentdb_core.bson;
    
    RETURN documentdb_api.insert_one(database_name, table_name, bson_doc);
END;
$$;

COMMENT ON FUNCTION documentdb_docsql.insert_into IS 
'SQL DML interface for inserting documents into collections';
