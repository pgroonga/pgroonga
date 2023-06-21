-- Upgrade SQL

CREATE FUNCTION pgroonga_equal_query_text_array_condition
	(targets text[], condition pgroonga_full_text_search_condition)
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
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

ALTER OPERATOR FAMILY pgroonga_text_array_term_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 41 &=~ (text[], pgroonga_full_text_search_condition);


CREATE FUNCTION pgroonga_equal_query_varchar_array_condition
	(targets varchar[], condition pgroonga_full_text_search_condition)
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
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

ALTER OPERATOR FAMILY pgroonga_varchar_array_term_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 41 &=~ (varchar[], pgroonga_full_text_search_condition);
