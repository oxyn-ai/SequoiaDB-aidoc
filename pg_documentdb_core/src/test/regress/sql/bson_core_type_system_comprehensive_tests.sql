SET search_path TO documentdb_core;


DROP TABLE IF EXISTS bson_type_test_table CASCADE;
DROP TABLE IF EXISTS bson_operator_test_table CASCADE;
DROP TABLE IF EXISTS bson_performance_test_table CASCADE;


CREATE TABLE bson_type_test_table (
    test_id SERIAL PRIMARY KEY,
    test_name TEXT,
    document bson
);

INSERT INTO bson_type_test_table (test_name, document) VALUES 
('int32_basic', '{"_id": 1, "value": {"$numberInt": "42"}}'),
('int32_max', '{"_id": 2, "value": {"$numberInt": "2147483647"}}'),
('int32_min', '{"_id": 3, "value": {"$numberInt": "-2147483648"}}'),
('int64_basic', '{"_id": 4, "value": {"$numberLong": "9223372036854775807"}}'),
('int64_min', '{"_id": 5, "value": {"$numberLong": "-9223372036854775808"}}'),
('double_basic', '{"_id": 6, "value": {"$numberDouble": "3.14159"}}'),
('double_max', '{"_id": 7, "value": {"$numberDouble": "1.7976931348623157E+308"}}'),
('double_min', '{"_id": 8, "value": {"$numberDouble": "-1.7976931348623157E+308"}}'),
('double_epsilon', '{"_id": 9, "value": {"$numberDouble": "4.94065645841247E-324"}}'),
('double_infinity', '{"_id": 10, "value": {"$numberDouble": "Infinity"}}'),
('double_neg_infinity', '{"_id": 11, "value": {"$numberDouble": "-Infinity"}}'),
('double_nan', '{"_id": 12, "value": {"$numberDouble": "NaN"}}');

INSERT INTO bson_type_test_table (test_name, document) VALUES 
('string_basic', '{"_id": 13, "value": "Hello World"}'),
('string_empty', '{"_id": 14, "value": ""}'),
('string_unicode', '{"_id": 15, "value": "Hello 世界 🌍"}'),
('string_special_chars', '{"_id": 16, "value": "Special chars: !@#$%^&*()_+-=[]{}|;:,.<>?"}'),
('string_long', '{"_id": 17, "value": "' || repeat('A', 1000) || '"}');

INSERT INTO bson_type_test_table (test_name, document) VALUES 
('boolean_true', '{"_id": 18, "value": true}'),
('boolean_false', '{"_id": 19, "value": false}'),
('null_value', '{"_id": 20, "value": null}'),
('undefined_field', '{"_id": 21}');

INSERT INTO bson_type_test_table (test_name, document) VALUES 
('date_basic', '{"_id": 22, "value": {"$date": {"$numberLong": "1609459200000"}}}'),
('date_before_epoch', '{"_id": 23, "value": {"$date": {"$numberLong": "-1577923200000"}}}'),
('timestamp_basic', '{"_id": 24, "value": {"$timestamp": {"t": 1565545664, "i": 1}}}');

INSERT INTO bson_type_test_table (test_name, document) VALUES 
('objectid_basic', '{"_id": 25, "value": {"$oid": "507f1f77bcf86cd799439011"}}'),
('objectid_different', '{"_id": 26, "value": {"$oid": "507f191e810c19729de860ea"}}');

INSERT INTO bson_type_test_table (test_name, document) VALUES 
('binary_basic', '{"_id": 27, "value": {"$binary": {"base64": "SGVsbG8gV29ybGQ=", "subType": "00"}}}'),
('binary_different_subtype', '{"_id": 28, "value": {"$binary": {"base64": "VGVzdCBEYXRh", "subType": "02"}}}');

INSERT INTO bson_type_test_table (test_name, document) VALUES 
('array_empty', '{"_id": 29, "value": []}'),
('array_numbers', '{"_id": 30, "value": [1, 2, 3, 4, 5]}'),
('array_mixed_types', '{"_id": 31, "value": [1, "string", true, null, {"nested": "object"}]}'),
('array_nested', '{"_id": 32, "value": [[1, 2], [3, 4], [5, 6]]}'),
('array_large', '{"_id": 33, "value": [' || string_agg(i::text, ',') || ']}')
FROM generate_series(1, 100) i;

INSERT INTO bson_type_test_table (test_name, document) VALUES 
('object_empty', '{"_id": 34, "value": {}}'),
('object_simple', '{"_id": 35, "value": {"name": "John", "age": 30}}'),
('object_nested', '{"_id": 36, "value": {"person": {"name": "Jane", "address": {"city": "NYC", "zip": "10001"}}}}'),
('object_complex', '{"_id": 37, "value": {"mixed": {"numbers": [1, 2, 3], "text": "hello", "flag": true, "nested": {"deep": {"value": 42}}}}}');

INSERT INTO bson_type_test_table (test_name, document) VALUES 
('minkey_basic', '{"_id": 38, "value": {"$minKey": 1}}'),
('maxkey_basic', '{"_id": 39, "value": {"$maxKey": 1}}');


\copy bson_type_test_table to '/tmp/bson_type_test.bin' with (format 'binary')
CREATE TABLE bson_type_test_copy (LIKE bson_type_test_table);
\copy bson_type_test_copy from '/tmp/bson_type_test.bin' with (format 'binary')

SELECT 'Serialization Test' as test_category, 
       CASE WHEN COUNT(*) = 0 THEN 'PASS' ELSE 'FAIL' END as result,
       COUNT(*) as differences
FROM (
    (TABLE bson_type_test_table EXCEPT TABLE bson_type_test_copy)
    UNION
    (TABLE bson_type_test_copy EXCEPT TABLE bson_type_test_table)
) q;

SELECT 'Hex Conversion Test' as test_category,
       test_name,
       CASE WHEN bson_hex_to_bson(bson_to_bson_hex(document)) = document 
            THEN 'PASS' ELSE 'FAIL' END as result
FROM bson_type_test_table 
WHERE test_id <= 10;


CREATE TABLE bson_operator_test_table (
    test_id SERIAL PRIMARY KEY,
    test_name TEXT,
    left_doc bson,
    right_doc bson
);

INSERT INTO bson_operator_test_table (test_name, left_doc, right_doc) VALUES 
('int_equal', '{"value": 42}', '{"value": 42}'),
('int_not_equal', '{"value": 42}', '{"value": 43}'),
('int_greater', '{"value": 43}', '{"value": 42}'),
('int_less', '{"value": 42}', '{"value": 43}'),
('double_equal', '{"value": 3.14}', '{"value": 3.14}'),
('mixed_numeric_equal', '{"value": 42}', '{"value": 42.0}'),
('mixed_numeric_not_equal', '{"value": 42}', '{"value": 42.1}'),

('string_equal', '{"value": "hello"}', '{"value": "hello"}'),
('string_not_equal', '{"value": "hello"}', '{"value": "world"}'),
('string_greater', '{"value": "world"}', '{"value": "hello"}'),
('string_case_sensitive', '{"value": "Hello"}', '{"value": "hello"}'),

('bool_equal_true', '{"value": true}', '{"value": true}'),
('bool_equal_false', '{"value": false}', '{"value": false}'),
('bool_not_equal', '{"value": true}', '{"value": false}'),

('null_equal', '{"value": null}', '{"value": null}'),
('null_vs_missing', '{"value": null}', '{}'),
('null_vs_value', '{"value": null}', '{"value": 42}'),

('array_equal', '{"value": [1, 2, 3]}', '{"value": [1, 2, 3]}'),
('array_not_equal', '{"value": [1, 2, 3]}', '{"value": [1, 2, 4]}'),
('array_different_length', '{"value": [1, 2, 3]}', '{"value": [1, 2]}'),

('object_equal', '{"value": {"a": 1, "b": 2}}', '{"value": {"a": 1, "b": 2}}'),
('object_field_order', '{"value": {"a": 1, "b": 2}}', '{"value": {"b": 2, "a": 1}}'),
('object_not_equal', '{"value": {"a": 1, "b": 2}}', '{"value": {"a": 1, "b": 3}}'),

('nan_equal', '{"value": {"$numberDouble": "NaN"}}', '{"value": {"$numberDouble": "NaN"}}'),
('infinity_equal', '{"value": {"$numberDouble": "Infinity"}}', '{"value": {"$numberDouble": "Infinity"}}'),
('infinity_vs_number', '{"value": {"$numberDouble": "Infinity"}}', '{"value": 1000000}');

SELECT 'Equality Tests' as test_category, test_name,
       extension_bson_equal(left_doc, right_doc) as equal_result,
       extension_bson_not_equal(left_doc, right_doc) as not_equal_result,
       extension_bson_compare(left_doc, right_doc) as compare_result
FROM bson_operator_test_table
WHERE test_name LIKE '%equal%';

SELECT 'Greater Than Tests' as test_category, test_name,
       extension_bson_gt(left_doc, right_doc) as gt_result,
       extension_bson_gte(left_doc, right_doc) as gte_result,
       extension_bson_compare(left_doc, right_doc) as compare_result
FROM bson_operator_test_table
WHERE test_name LIKE '%greater%' OR test_name LIKE '%less%';

SELECT 'Less Than Tests' as test_category, test_name,
       extension_bson_lt(left_doc, right_doc) as lt_result,
       extension_bson_lte(left_doc, right_doc) as lte_result,
       extension_bson_compare(left_doc, right_doc) as compare_result
FROM bson_operator_test_table
WHERE test_name LIKE '%less%' OR test_name LIKE '%greater%';


SELECT 'Error Handling Tests' as test_category;

DO $$
BEGIN
    BEGIN
        PERFORM '{"invalid": json}'::bson;
        RAISE NOTICE 'Invalid BSON test: Should have failed';
    EXCEPTION WHEN OTHERS THEN
        RAISE NOTICE 'Invalid BSON test: Correctly caught error - %', SQLERRM;
    END;
END $$;

SELECT 'Type Checking Tests' as test_category,
       test_name,
       CASE 
           WHEN document ? 'value' THEN 'HAS_VALUE'
           ELSE 'NO_VALUE'
       END as has_value_check
FROM bson_type_test_table
WHERE test_id <= 5;


SELECT 'Hash Consistency Tests' as test_category;

SELECT 'Numeric Hash Consistency' as subtest,
       bson_hash_int4('{"": {"$numberInt": "42"}}') = bson_hash_int4('{"": {"$numberLong": "42"}}') as int_long_hash_equal,
       bson_hash_int4('{"": {"$numberInt": "42"}}') = bson_hash_int4('{"": 42.0}') as int_double_hash_equal;

SELECT 'Hash Distribution Test' as subtest,
       COUNT(DISTINCT bson_hash_int4(document)) as unique_hashes,
       COUNT(*) as total_documents,
       ROUND(COUNT(DISTINCT bson_hash_int4(document))::numeric / COUNT(*)::numeric * 100, 2) as distribution_percentage
FROM bson_type_test_table;


CREATE TABLE bson_performance_test_table (
    test_id SERIAL PRIMARY KEY,
    document bson
);

INSERT INTO bson_performance_test_table (document)
SELECT ('{"_id": ' || i || ', "data": "' || repeat('x', 1000) || '", "numbers": [' || 
        string_agg((random() * 1000)::int::text, ',') || '], "nested": {"level1": {"level2": {"level3": {"value": ' || i || '}}}}}')::bson
FROM generate_series(1, 100) i, generate_series(1, 10) j
GROUP BY i;

\timing on
SELECT 'Performance Test: Large Document Comparison' as test_category,
       COUNT(*) as comparisons_performed
FROM bson_performance_test_table a, bson_performance_test_table b
WHERE a.test_id <= 10 AND b.test_id <= 10 AND extension_bson_equal(a.document, b.document);
\timing off

\timing on
SELECT 'Performance Test: Hash Computation' as test_category,
       COUNT(DISTINCT bson_hash_int4(document)) as unique_hashes
FROM bson_performance_test_table
WHERE test_id <= 50;
\timing off


SELECT 'Edge Case Tests' as test_category;

INSERT INTO bson_type_test_table (test_name, document) VALUES 
('edge_empty_object', '{}'),
('edge_only_id', '{"_id": 1}'),
('edge_null_in_array', '{"arr": [1, null, 3]}'),
('edge_empty_string_key', '{"": "empty_key_value"}'),
('edge_nested_nulls', '{"a": {"b": {"c": null}}}'),
('edge_mixed_array', '{"arr": [{"a": 1}, null, "string", 42, true]}');

SELECT 'Edge Case Comparison Tests' as subtest,
       extension_bson_equal('{}', '{}') as empty_objects_equal,
       extension_bson_equal('{"a": null}', '{"a": null}') as null_values_equal,
       extension_bson_equal('{"a": null}', '{}') as null_vs_missing_equal,
       extension_bson_compare('{"a": []}', '{"a": [null]}') as empty_vs_null_array;

SELECT 'Type Sort Order Tests' as subtest,
       extension_bson_compare('{"v": null}', '{"v": 1}') as null_vs_number,
       extension_bson_compare('{"v": 1}', '{"v": "1"}') as number_vs_string,
       extension_bson_compare('{"v": "a"}', '{"v": true}') as string_vs_boolean,
       extension_bson_compare('{"v": true}', '{"v": []}') as boolean_vs_array,
       extension_bson_compare('{"v": []}', '{"v": {}}') as array_vs_object;


SELECT 'Memory Management Tests' as test_category;

DO $$
DECLARE
    i INTEGER;
    result BOOLEAN;
BEGIN
    FOR i IN 1..1000 LOOP
        SELECT extension_bson_equal(
            ('{"test": ' || i || '}')::bson,
            ('{"test": ' || (i % 100) || '}')::bson
        ) INTO result;
    END LOOP;
    RAISE NOTICE 'Memory management test completed: 1000 comparisons performed';
END $$;


SELECT 'TEST SUMMARY' as category, 
       'Total test documents created' as metric,
       COUNT(*) as value
FROM bson_type_test_table;

SELECT 'TEST SUMMARY' as category,
       'Total comparison test cases' as metric,
       COUNT(*) as value
FROM bson_operator_test_table;

SELECT 'TEST SUMMARY' as category,
       'Performance test documents' as metric,
       COUNT(*) as value
FROM bson_performance_test_table;

SELECT 'COVERAGE SUMMARY' as category,
       'BSON types tested' as metric,
       string_agg(DISTINCT test_name, ', ' ORDER BY test_name) as types_covered
FROM bson_type_test_table
WHERE test_name LIKE '%_basic' OR test_name LIKE '%_empty' OR test_name LIKE 'minkey%' OR test_name LIKE 'maxkey%';

DROP TABLE IF EXISTS bson_type_test_copy;

SELECT 'BSON Core Type System Comprehensive Tests Completed Successfully' as final_status;
