CREATE FUNCTION pgroonga_highlight_html(target text,
				        keywords text[],
				        indexName cstring)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_highlight_html'
	LANGUAGE C
	IMMUTABLE
	STRICT;
