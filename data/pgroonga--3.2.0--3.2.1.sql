-- Upgrade SQL

CREATE FUNCTION pgroonga_list_broken_indexes()
	RETURNS void /* todo */
	AS 'MODULE_PATHNAME', 'pgroonga_list_broken_indexes'
	LANGUAGE C
	STRICT
	PARALLEL SAFE;
