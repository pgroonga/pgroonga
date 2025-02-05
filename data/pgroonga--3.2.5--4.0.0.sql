-- Upgrade SQL

DROP OPERATOR FAMILY pgroonga.text_full_text_search_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.text_array_full_text_search_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.varchar_full_text_search_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.varchar_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.varchar_array_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.bool_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.int2_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.int4_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.int8_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.timestamp_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.timestamptz_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.jsonb_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.text_regexp_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.varchar_regexp_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.text_full_text_search_ops_v2 USING pgroonga;
DROP OPERATOR FAMILY pgroonga.text_array_full_text_search_ops_v2 USING pgroonga;
DROP OPERATOR FAMILY pgroonga.text_term_search_ops_v2 USING pgroonga;
DROP OPERATOR FAMILY pgroonga.text_array_term_search_ops_v2 USING pgroonga;
DROP OPERATOR FAMILY pgroonga.text_regexp_ops_v2 USING pgroonga;
DROP OPERATOR FAMILY pgroonga.varchar_full_text_search_ops_v2 USING pgroonga;
DROP OPERATOR FAMILY pgroonga.varchar_array_term_search_ops_v2 USING pgroonga;
DROP OPERATOR FAMILY pgroonga.varchar_regexp_ops_v2 USING pgroonga;

DROP FUNCTION pgroonga.query_expand(tableName cstring,
                                    termColumnName text,
                                    synonymsColumnName text,
                                    query text);
