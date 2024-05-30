-- Upgrade SQL

ALTER TYPE pgroonga_condition ADD ATTRIBUTE fuzzy_max_distance_ratio float4;
CREATE OR REPLACE FUNCTION pgroonga_condition(query text = null,
				   weights int[] = null,
				   scorers text[] = null,
				   schema_name text = null,
				   index_name text = null,
				   column_name text = null,
				   fuzzy_max_distance_ratio float4 = null)
	RETURNS pgroonga_condition
	LANGUAGE SQL
	AS $$
		SELECT (
			query,
			weights,
			scorers,
			schema_name,
			index_name,
			column_name,
			fuzzy_max_distance_ratio
		)::pgroonga_condition
	$$
	IMMUTABLE
	LEAKPROOF
	PARALLEL SAFE;

DO LANGUAGE plpgsql $$
DECLARE
	rec record;
BEGIN
	-- A column in pg_stat_wal_receiver is different for 12 and 13 or higher.
	--* received_lsn: 12
	--* flushed_lsn: 13 or higher
	SELECT * INTO STRICT rec from pg_settings WHERE name = 'server_version';
	IF SUBSTR(rec.setting, 1, 2) >= '13' THEN
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
	ELSE
		CREATE FUNCTION pgroonga_list_lagged_indexes()
			RETURNS SETOF text AS '
				SELECT name FROM pgroonga_wal_status()
				WHERE current_block != last_block
					OR current_offset != current_offset
					OR current_size != last_size
					OR EXISTS(
						SELECT 1 FROM pg_stat_wal_receiver
						WHERE received_lsn != latest_end_lsn
					);
			' LANGUAGE SQL
			STRICT
			PARALLEL SAFE;
	END IF;
END;
$$;

CREATE FUNCTION pgroonga_list_broken_indexes()
	RETURNS SETOF text
	AS 'MODULE_PATHNAME', 'pgroonga_list_broken_indexes'
	LANGUAGE C
	STRICT
	PARALLEL SAFE;
