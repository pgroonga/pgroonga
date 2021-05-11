-- Upgrade SQL

CREATE FUNCTION pgroonga_result_to_recordset(result jsonb)
	RETURNS SETOF RECORD
	AS 'MODULE_PATHNAME', 'pgroonga_result_to_recordset'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_result_to_jsonb_objects(result jsonb)
	RETURNS jsonb
	AS 'MODULE_PATHNAME', 'pgroonga_result_to_jsonb_objects'
	LANGUAGE C
	IMMUTABLE
	STRICT;
