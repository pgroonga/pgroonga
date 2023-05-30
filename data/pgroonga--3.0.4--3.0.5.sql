-- Upgrade SQL

CREATE FUNCTION pgroonga_wal_set_applied_position(indexName cstring, "block" bigint, "offset" bigint)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_wal_set_applied_position_index'
	LANGUAGE C
	STRICT;

CREATE FUNCTION pgroonga_wal_set_applied_position(indexName cstring)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_wal_set_applied_position_index_last'
	LANGUAGE C
	STRICT;

CREATE FUNCTION pgroonga_wal_set_applied_position("block" bigint, "offset" bigint)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_wal_set_applied_position_all'
	LANGUAGE C
	STRICT;

CREATE FUNCTION pgroonga_wal_set_applied_position()
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_wal_set_applied_position_all_last'
	LANGUAGE C
	STRICT;
