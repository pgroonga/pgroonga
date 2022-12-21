-- Upgrade SQL

CREATE FUNCTION pgroonga_highlight_html(targets text[], keywords text[])
	RETURNS text[]
	AS 'MODULE_PATHNAME', 'pgroonga_highlight_html_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_highlight_html(targets text[],
				        keywords text[],
				        indexName cstring)
	RETURNS text[]
	AS 'MODULE_PATHNAME', 'pgroonga_highlight_html_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;
