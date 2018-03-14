UPDATE pg_catalog.pg_proc
   SET prosrc = 'pgroonga_score_row'
 WHERE proname = 'pgroonga_score';

CREATE FUNCTION pgroonga_score(tableoid oid, ctid tid)
	RETURNS float8
	AS 'MODULE_PATHNAME', 'pgroonga_score_ctid'
	LANGUAGE C
	VOLATILE
	STRICT;


CREATE TYPE pgroonga_full_text_search_condition AS (
  query text,
  weigths int[],
  indexName text
);

CREATE FUNCTION pgroonga_match_text_condition
	(target text, condition pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_text_condition,
	LEFTARG = text,
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_varchar_condition
	(target varchar, condition pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_varchar_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_varchar_condition,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_text_array_condition
	(target text[], condition pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_text_array_condition,
	LEFTARG = text[],
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_text_condition
	(target text, condition pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_text_condition,
	LEFTARG = text,
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_text_array_condition
	(targets text[], condition pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_text_array_condition,
	LEFTARG = text[],
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

-- Update amstrategies for old PostgreSQL
DO LANGUAGE plpgsql $$
BEGIN
	UPDATE pg_am SET amstrategies = 32
	 WHERE amname = 'pgroonga';
EXCEPTION
	WHEN undefined_column THEN -- Ignore
END;
$$;

ALTER OPERATOR FAMILY pgroonga_text_full_text_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 31 &@ (text, pgroonga_full_text_search_condition),
		OPERATOR 32 &@~ (text, pgroonga_full_text_search_condition);

ALTER OPERATOR FAMILY pgroonga_varchar_full_text_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 31 &@ (varchar, pgroonga_full_text_search_condition);

ALTER OPERATOR FAMILY pgroonga_text_array_full_text_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 31 &@ (text[], pgroonga_full_text_search_condition),
		OPERATOR 32 &@~ (text[], pgroonga_full_text_search_condition);
