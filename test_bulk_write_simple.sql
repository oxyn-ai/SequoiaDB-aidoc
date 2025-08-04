
\echo 'Testing bulk_write function existence...'

SELECT proname, pronargs 
FROM pg_proc p 
JOIN pg_namespace n ON p.pronamespace = n.oid 
WHERE n.nspname = 'documentdb_api' AND p.proname = 'bulk_write';

\echo 'If the above query returns a row, the bulk_write function is properly installed.'

/*
SELECT documentdb_api.create_collection('testdb', 'test_collection');

SELECT documentdb_api.bulk_write('testdb', '{ "bulkWrite": "test_collection" }', 
    '[
        { "insertOne": { "document": { "_id": 1, "name": "Alice", "age": 25 } } },
        { "insertOne": { "document": { "_id": 2, "name": "Bob", "age": 30 } } },
        { "updateOne": { "filter": { "_id": 1 }, "update": { "$set": { "age": 26 } } } },
        { "deleteOne": { "filter": { "_id": 2 } } }
    ]'::bsonsequence);

SELECT COUNT(*) FROM documentdb_api.collection('testdb', 'test_collection');
SELECT document FROM documentdb_api.collection('testdb', 'test_collection');
*/
