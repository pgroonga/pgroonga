CREATE FUNCTION pgroonga_tokenize(target text, VARIADIC options text[])
	RETURNS text[]
	AS 'MODULE_PATHNAME', 'pgroonga_tokenize'
	LANGUAGE C
	IMMUTABLE
	STRICT;
