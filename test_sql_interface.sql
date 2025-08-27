
\echo 'Testing pg_documentdb_docsql Extension - Pure SQL Interface for DocumentDB'
\echo '========================================================================='

\echo 'Extension Version:'
SELECT documentdb_docsql.get_version();

\echo ''
\echo 'Testing DDL Operations:'
\echo '----------------------'

\echo 'Creating table "users":'
SELECT documentdb_docsql.create_table('testdb', 'users') as created;

\echo ''
\echo 'Testing DML Operations:'
\echo '----------------------'

\echo 'Inserting user record:'
SELECT documentdb_docsql.insert_into('testdb', 'users', 
    ARRAY['name', 'age', 'email', 'city'], 
    ARRAY['John Doe', '30', 'john@example.com', 'New York']) as inserted_document;

\echo 'Inserting another user record:'
SELECT documentdb_docsql.insert_into('testdb', 'users', 
    ARRAY['name', 'age', 'email', 'city'], 
    ARRAY['Jane Smith', '25', 'jane@example.com', 'San Francisco']) as inserted_document;

\echo ''
\echo 'Querying user records:'
SELECT * FROM documentdb_docsql.select_from('testdb', 'users', '{"name": "John Doe"}') as user_record;

\echo 'Querying all users (empty filter):'
SELECT * FROM documentdb_docsql.select_from('testdb', 'users', '{}') as user_records;

\echo ''
\echo 'Updating user record:'
SELECT documentdb_docsql.update_table('testdb', 'users', 
    '{"name": "John Doe"}', 
    '{"$set": {"age": 31, "city": "Boston"}}') as update_result;

\echo ''
\echo 'Deleting user record:'
SELECT documentdb_docsql.delete_from('testdb', 'users', '{"name": "Jane Smith"}') as delete_result;

\echo ''
\echo 'Dropping table "users":'
SELECT documentdb_docsql.drop_table('testdb', 'users') as dropped;

\echo ''
\echo 'SQL Interface Test Complete!'
\echo 'All operations demonstrate the pure SQL interface for DocumentDB.'
