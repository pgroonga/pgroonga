-- Upgrade SQL

CREATE FUNCTION pgroonga_regexp_text_array(targets text[], pattern text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_regexp_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &~ (
	PROCEDURE = pgroonga_regexp_text_array,
	LEFTARG = text[],
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_regexp_text_array_condition
        (targets text[], pattern pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_regexp_text_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &~ (
	PROCEDURE = pgroonga_regexp_text_array_condition,
	LEFTARG = text[],
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR CLASS pgroonga_text_array_regexp_ops_v2 FOR TYPE text[]
	USING pgroonga AS
		OPERATOR 22 &~ (text[], text),
		OPERATOR 47 &~ (text[], pgroonga_condition);

CREATE OR REPLACE FUNCTION pgroonga_list_lagged_indexes()
	RETURNS SETOF text AS '
		SELECT name FROM pgroonga_wal_status()
		WHERE current_block != last_block
			OR current_offset != last_offset
			OR current_size != last_size
			OR EXISTS(
				SELECT 1 FROM pg_stat_wal_receiver
				WHERE flushed_lsn != latest_end_lsn
			);
	' LANGUAGE SQL
	STRICT
	PARALLEL SAFE;
