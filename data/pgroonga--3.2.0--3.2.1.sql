-- Upgrade SQL

CREATE FUNCTION pgroonga_list_lagged_indexes()
	RETURNS void /* todo */
	AS 'MODULE_PATHNAME', 'pgroonga_list_lagged_indexes'
	LANGUAGE C
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_list_broken_indexes()
	RETURNS SETOF text
	AS 'MODULE_PATHNAME', 'pgroonga_list_broken_indexes'
	LANGUAGE C
	STRICT
	PARALLEL SAFE;
