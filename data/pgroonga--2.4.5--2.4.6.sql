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


CREATE FUNCTION pgroonga_equal_text(target text, other text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &= (
	PROCEDURE = pgroonga_equal_text,
	LEFTARG = text,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_equal_text_condition
	(target text, condition pgroonga_full_text_search_condition)
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
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_equal_varchar(target varchar, other varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &= (
	PROCEDURE = pgroonga_equal_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_equal_varchar_condition
	(target varchar, condition pgroonga_full_text_search_condition)
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
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

ALTER OPERATOR FAMILY pgroonga_text_term_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 38 &= (text, text),
		OPERATOR 39 &= (text, pgroonga_full_text_search_condition);

ALTER OPERATOR FAMILY pgroonga_varchar_term_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 38 &= (varchar, varchar),
		OPERATOR 39 &= (varchar, pgroonga_full_text_search_condition);
