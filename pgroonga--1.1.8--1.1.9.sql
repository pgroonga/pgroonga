CREATE FUNCTION pgroonga.escape(target text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(target text, special_characters text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape'
	LANGUAGE C
	IMMUTABLE
	STRICT;
