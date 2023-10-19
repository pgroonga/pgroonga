-- Upgrade SQL

CREATE FUNCTION pgroonga_snippet_html(target text, keywords text[], width integer DEFAULT 200)
	RETURNS text[]
	AS 'MODULE_PATHNAME', 'pgroonga_snippet_html'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;
DROP FUNCTION pgroonga_snippet_html(target text, keywords text[]);
