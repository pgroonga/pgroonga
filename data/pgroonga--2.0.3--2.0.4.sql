UPDATE pg_catalog.pg_proc
   SET prosrc = 'pgroonga_score_row'
 WHERE proname = 'pgroonga_score';

CREATE FUNCTION pgroonga_score(tableoid oid, ctid tid)
	RETURNS float8
	AS 'MODULE_PATHNAME', 'pgroonga_score_ctid'
	LANGUAGE C
	VOLATILE
	STRICT;
