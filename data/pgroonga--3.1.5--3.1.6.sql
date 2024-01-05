-- Upgrade SQL

CREATE TYPE pgroonga_condition AS (
	query text,
	weigths int[],
	scorers text[],
	schema_name text,
	index_name text,
	column_name text
);

CREATE FUNCTION pgroonga_condition(query text = null,
				   weights int[] = null,
				   scorers text[] = null,
				   schema_name text = null,
				   index_name text = null,
				   column_name text = null)
	RETURNS pgroonga_condition
	LANGUAGE SQL
	AS $$
		SELECT (
			query,
			weights,
			scorers,
			schema_name,
			index_name,
			column_name
		)::pgroonga_condition
	$$
	IMMUTABLE
	LEAKPROOF
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_match_text_condition
	(target text, condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_text_condition,
	LEFTARG = text,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_text_array_condition
	(target text[], condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_text_array_condition,
	LEFTARG = text[],
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_varchar_condition
	(target varchar, condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_varchar_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_varchar_condition,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_text_condition
	(target text, condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_text_condition,
	LEFTARG = text,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_text_array_condition
	(targets text[], condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_text_array_condition,
	LEFTARG = text[],
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_varchar_condition
	(target varchar, condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_varchar_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_varchar_condition,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_text_condition
	(text, condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_text_condition,
	LEFTARG = text,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_text_array_condition(text[], pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_text_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_text_array_condition,
	LEFTARG = text[],
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_varchar_condition
	(target varchar, conditoin pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_varchar_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_varchar_condition,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_varchar_array_condition(varchar[], pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_varchar_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_varchar_array_condition,
	LEFTARG = varchar[],
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_equal_text_condition
	(target text, condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &= (
	PROCEDURE = pgroonga_equal_text_condition,
	LEFTARG = text,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_equal_varchar_condition
	(target varchar, condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &= (
	PROCEDURE = pgroonga_equal_varchar_condition,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_equal_query_text_array_condition
	(targets text[], condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_query_text_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &=~ (
	PROCEDURE = pgroonga_equal_query_text_array_condition,
	LEFTARG = text[],
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_equal_query_varchar_array_condition
	(targets varchar[], condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_query_varchar_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &=~ (
	PROCEDURE = pgroonga_equal_query_varchar_array_condition,
	LEFTARG = varchar[],
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

ALTER OPERATOR FAMILY pgroonga_text_full_text_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 42 &@ (text, pgroonga_condition),
		OPERATOR 43 &@~ (text, pgroonga_condition);

ALTER OPERATOR FAMILY pgroonga_text_array_full_text_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 42 &@ (text[], pgroonga_condition),
		OPERATOR 43 &@~ (text[], pgroonga_condition);

ALTER OPERATOR FAMILY pgroonga_text_term_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 44 &^ (text, pgroonga_condition),
		OPERATOR 45 &= (text, pgroonga_condition);

ALTER OPERATOR FAMILY pgroonga_text_array_term_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 44 &^ (text[], pgroonga_condition),
		OPERATOR 46 &=~ (text[], pgroonga_condition);

ALTER OPERATOR FAMILY pgroonga_varchar_full_text_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 42 &@ (varchar, pgroonga_condition),
		OPERATOR 43 &@~ (varchar, pgroonga_condition);

ALTER OPERATOR FAMILY pgroonga_varchar_array_term_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 44 &^ (varchar[], pgroonga_condition),
		OPERATOR 46 &=~ (varchar[], pgroonga_condition);
