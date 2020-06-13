-- Upgrade SQL

CREATE FUNCTION pgroonga_index_column_name(indexName cstring, columnName text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_index_column_name_name'
	LANGUAGE C
	STABLE
	STRICT;

CREATE FUNCTION pgroonga_index_column_name(indexName cstring, columnIndex int4)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_index_column_name_index'
	LANGUAGE C
	STABLE
	STRICT;
