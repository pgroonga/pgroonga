CREATE FUNCTION pgroonga.query_expand(tableName cstring,
				      termColumnName text,
				      synonymsColumnName text,
				      term text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_query_expand'
	LANGUAGE C
	STABLE
	STRICT;

ALTER OPERATOR FAMILY pgroonga.text_term_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 1 < (text, text),
		OPERATOR 2 <= (text, text),
		OPERATOR 3 = (text, text),
		OPERATOR 4 >= (text, text),
		OPERATOR 5 > (text, text);
