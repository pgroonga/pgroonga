-- Upgrade SQL

DROP OPERATOR FAMILY pgroonga.text_full_text_search_ops USING pgroonga;
DROP OPERATOR CLASS pgroonga.text_full_text_search_ops USING pgroonga;
