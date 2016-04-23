CREATE FUNCTION pgroonga.highlight_html(target text, keywords text[])
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_highlight_html'
	LANGUAGE C
	VOLATILE
	STRICT;
