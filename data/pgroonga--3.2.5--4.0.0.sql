-- Upgrade SQL

DROP OPERATOR FAMILY pgroonga.text_full_text_search_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.text_array_full_text_search_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.varchar_full_text_search_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.varchar_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.varchar_array_ops USING pgroonga;
