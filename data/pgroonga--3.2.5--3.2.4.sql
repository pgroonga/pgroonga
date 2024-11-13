-- Downgrade SQL

DROP FUNCTION pgroonga_regexp_text_array;
DROP OPERATOR CLASS pgroonga_text_array_regexp_ops_v2 USING pgroonga;
DROP OPERATOR FAMILY pgroonga_text_array_regexp_ops_v2 USING pgroonga;
