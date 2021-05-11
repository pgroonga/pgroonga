-- Upgrade SQL

CREATE FUNCTION pgroonga_result_to_recordset(result jsonb)
	RETURNS SETOF RECORD
	AS 'MODULE_PATHNAME', 'pgroonga_result_to_recordset'
	LANGUAGE C
	IMMUTABLE
	STRICT;
