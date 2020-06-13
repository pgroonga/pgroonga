-- Upgrade SQL

CREATE FUNCTION pgroonga_index_column_name(indexName text, columnName text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_index_column_name_string'
	LANGUAGE C
	STABLE
	STRICT;

CREATE FUNCTION pgroonga_index_column_name(indexName text, i int4)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_index_column_name_int4'
	LANGUAGE C
	STABLE
	STRICT;
