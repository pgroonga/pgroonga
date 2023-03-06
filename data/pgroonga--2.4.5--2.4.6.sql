-- Upgrade SQL

CREATE FUNCTION pgroonga_prefix_text_condition
	(text, condition pgroonga_full_text_search_condition)
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
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_varchar_condition
	(target varchar, conditoin pgroonga_full_text_search_condition)
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
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

ALTER OPERATOR FAMILY pgroonga_text_term_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 37 &^ (text, pgroonga_full_text_search_condition);

ALTER OPERATOR FAMILY pgroonga_varchar_term_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 37 &^ (varchar, pgroonga_full_text_search_condition);
