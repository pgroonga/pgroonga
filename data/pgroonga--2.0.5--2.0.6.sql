CREATE TYPE pgroonga_full_text_search_condition_with_scorers AS (
  query text,
  weigths int[],
  scorers text[],
  indexName text
);


-- Update amstrategies for old PostgreSQL
DO LANGUAGE plpgsql $$
BEGIN
	UPDATE pg_am SET amstrategies = 34
	 WHERE amname = 'pgroonga';
EXCEPTION
	WHEN undefined_column THEN -- Ignore
END;
$$;


CREATE FUNCTION pgroonga_match_text_condition_with_scorers
	(target text,
	 condition pgroonga_full_text_search_condition_with_scorers)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text_condition_with_scorers'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_text_condition_with_scorers,
	LEFTARG = text,
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_text_condition_with_scorers
	(target text,
	 condition pgroonga_full_text_search_condition_with_scorers)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text_condition_with_scorers'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_text_condition_with_scorers,
	LEFTARG = text,
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_varchar_condition_with_scorers
	(target varchar,
	 condition pgroonga_full_text_search_condition_with_scorers)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_varchar_condition_with_scorers'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_varchar_condition_with_scorers,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_varchar_condition
	(target varchar, condition pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_varchar_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_varchar_condition,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_varchar_condition_with_scorers
	(target varchar,
	 condition pgroonga_full_text_search_condition_with_scorers)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_varchar_condition_with_scorers'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_varchar_condition_with_scorers,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_text_array_condition_with_scorers
	(target text[],
	 condition pgroonga_full_text_search_condition_with_scorers)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text_array_condition_with_scorers'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_text_array_condition_with_scorers,
	LEFTARG = text[],
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_text_array_condition_with_scorers
	(targets text[],
	 condition pgroonga_full_text_search_condition_with_scorers)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text_array_condition_with_scorers'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_text_array_condition_with_scorers,
	LEFTARG = text[],
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);


ALTER OPERATOR FAMILY pgroonga_text_full_text_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 33 &@ (text, pgroonga_full_text_search_condition_with_scorers),
		OPERATOR 34 &@~ (text, pgroonga_full_text_search_condition_with_scorers);

ALTER OPERATOR FAMILY pgroonga_varchar_full_text_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 32 &@~ (varchar, pgroonga_full_text_search_condition),
		OPERATOR 33 &@ (varchar, pgroonga_full_text_search_condition_with_scorers),
		OPERATOR 34 &@~ (varchar, pgroonga_full_text_search_condition_with_scorers);

ALTER OPERATOR FAMILY pgroonga_text_array_full_text_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 33 &@ (text[], pgroonga_full_text_search_condition_with_scorers),
		OPERATOR 34 &@~ (text[], pgroonga_full_text_search_condition_with_scorers);
