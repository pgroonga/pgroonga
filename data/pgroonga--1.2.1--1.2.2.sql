CREATE FUNCTION pgroonga.query_expand(tableName cstring,
				      termColumnName text,
				      synonymsColumnName text,
				      term text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_query_expand'
	LANGUAGE C
	STABLE
	STRICT;
