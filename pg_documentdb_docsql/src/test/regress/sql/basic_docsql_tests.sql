SELECT documentdb_docsql.create_table('testdb', 'users');

SELECT documentdb_docsql.insert_into('testdb', 'users', 
    ARRAY['name', 'age', 'email'], 
    ARRAY['John Doe', '30', 'john@example.com']);

SELECT * FROM documentdb_docsql.select_from('testdb', 'users', '{"name": "John Doe"}');

SELECT documentdb_docsql.update_table('testdb', 'users', 
    '{"name": "John Doe"}', 
    '{"$set": {"age": 31}}');

SELECT documentdb_docsql.delete_from('testdb', 'users', '{"name": "John Doe"}');

SELECT documentdb_docsql.create_index('testdb', 'users', 'idx_name', ARRAY['name']);
SELECT documentdb_docsql.drop_index('testdb', 'users', 'idx_name');

SELECT documentdb_docsql.drop_table('testdb', 'users');

SELECT documentdb_docsql.get_version();
