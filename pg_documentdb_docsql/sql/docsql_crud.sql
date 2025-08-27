CREATE EXTENSION IF NOT EXISTS pg_documentdb;
CREATE EXTENSION IF NOT EXISTS pg_documentdb_docsql;

SELECT documentdb_docsql.create_database('docdb1');
SELECT documentdb_docsql.create_collection('docdb1','patient');

INSERT INTO docsql_docdb1.patient(document)
VALUES ('{ "patient_id": "P001", "name": "Alice", "age": 30 }');

SELECT (document ->> 'patient_id') AS pid, (document ->> 'name') AS name
FROM docsql_docdb1.patient
ORDER BY pid;
