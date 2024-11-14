-- Downgrade SQL

DROP OPERATOR FAMILY pgroonga_text_array_regexp_ops_v2 USING pgroonga;
DROP OPERATOR &~ (text[], text);
DROP FUNCTION pgroonga_regexp_text_array;
