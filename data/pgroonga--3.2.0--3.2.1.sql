-- Upgrade SQL

CREATE FUNCTION pgroonga_list_lagged_indexes()
	RETURNS TABLE (name text) AS $$
		SELECT name FROM pgroonga_wal_status()
		WHERE current_block != last_block
			OR current_offset != current_offset
			OR current_size != last_size
		UNION
		SELECT name FROM pgroonga_wal_status()
		WHERE EXISTS(
			SELECT 1 FROM pg_stat_wal_receiver
			WHERE flushed_lsn != latest_end_lsn
		);
	$$ LANGUAGE SQL
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_list_broken_indexes()
	RETURNS SETOF text
	AS 'MODULE_PATHNAME', 'pgroonga_list_broken_indexes'
	LANGUAGE C
	STRICT
	PARALLEL SAFE;
