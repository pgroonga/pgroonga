-- Upgrade SQL

ALTER FUNCTION pgroonga_similar_text(text, text) COST 10000;
ALTER FUNCTION pgroonga_similar_text_array(text[], text) COST 10000;
ALTER FUNCTION pgroonga_similar_varchar(varchar, varchar) COST 10000;

CREATE FUNCTION pgroonga_match_positions_byte(target text,
					      keywords text[],
					      indexName cstring)
	RETURNS integer[2][]
	AS 'MODULE_PATHNAME', 'pgroonga_match_positions_byte'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_match_positions_character(target text,
						   keywords text[],
						   indexName cstring)
	RETURNS integer[2][]
	AS 'MODULE_PATHNAME', 'pgroonga_match_positions_character'
	LANGUAGE C
	IMMUTABLE
	STRICT;

