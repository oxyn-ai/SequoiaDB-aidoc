CREATE EXTENSION IF NOT EXISTS pg_documentdb;
CREATE EXTENSION IF NOT EXISTS pg_documentdb_docsql;

SELECT documentdb_docsql.create_database('docdb2');
SELECT documentdb_docsql.create_collection('docdb2','orders');

INSERT INTO docsql_docdb2.orders(document) VALUES
('{ "_id": "o1", "user": "u1", "total": 10 }'),
('{ "_id": "o2", "user": "u2", "total": 20 }');

UPDATE docsql_docdb2.orders
SET document = '{ "_id": "o1", "user": "u1", "total": 15 }'
WHERE (document->>'_id') = 'o1';

DELETE FROM docsql_docdb2.orders
WHERE (document->>'_id') = 'o2';

SELECT (document->>'_id') id, (document->>'total') total
FROM docsql_docdb2.orders
ORDER BY id;
