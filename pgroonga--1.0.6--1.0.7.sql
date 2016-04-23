CREATE FUNCTION pgroonga.highlight_html(target text, keywords text[])
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_highlight_html'
	LANGUAGE C
	VOLATILE
	STRICT;

CREATE FUNCTION pgroonga.match_positions_byte(target text, keywords text[])
	RETURNS integer[2][]
	AS 'MODULE_PATHNAME', 'pgroonga_match_positions_byte'
	LANGUAGE C
	VOLATILE
	STRICT;
