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

ALTER OPERATOR FAMILY pgroonga_text_array_full_text_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 34 &@~ (text[], pgroonga_full_text_search_condition_with_scorers);
