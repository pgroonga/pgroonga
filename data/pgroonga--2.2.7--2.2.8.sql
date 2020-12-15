-- Upgrade SQL

ALTER FUNCTION pgroonga_similar_text(text, text) COST 10000;
ALTER FUNCTION pgroonga_similar_text_array(text[], text) COST 10000;
ALTER FUNCTION pgroonga_similar_varchar(varchar, varchar) COST 10000;
