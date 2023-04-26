-- Upgrade SQL

CREATE FUNCTION pgroonga_wal_status()
	RETURNS TABLE(
			name text,
			oid oid,
			current_block int8,
			current_offset int8,
			current_size int8,
			last_block int8,
			last_offset int8,
			last_size int8
		)
	AS 'MODULE_PATHNAME', 'pgroonga_wal_status'
	LANGUAGE C
	STRICT
	PARALLEL SAFE;
