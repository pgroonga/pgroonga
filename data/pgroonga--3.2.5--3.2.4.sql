-- Downgrade SQL

DROP OPERATOR FAMILY pgroonga_text_array_regexp_ops_v2 USING pgroonga;
DROP OPERATOR &~ (text[], text);
DROP OPERATOR &~ (text[], pgroonga_condition);
DROP FUNCTION pgroonga_regexp_text_array;
DROP FUNCTION pgroonga_regexp_text_array_condition;

DROP FUNCTION pgroonga_list_lagged_indexes;
CREATE FUNCTION pgroonga_list_lagged_indexes()
	RETURNS SETOF text AS '
		SELECT name FROM pgroonga_wal_status()
		WHERE current_block != last_block
			OR current_offset != current_offset
			OR current_size != last_size
			OR EXISTS(
				SELECT 1 FROM pg_stat_wal_receiver
				WHERE flushed_lsn != latest_end_lsn
			);
	' LANGUAGE SQL
	STRICT
	PARALLEL SAFE;
