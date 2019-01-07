CREATE FUNCTION pgroonga_database_remove()
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_database_remove'
	LANGUAGE C
	VOLATILE
	STRICT;
