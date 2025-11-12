-- Upgrade SQL

CREATE OR REPLACE FUNCTION pgroonga_language_model_vectorize(model_name cstring, target text)
	RETURNS float4[]
	AS 'MODULE_PATHNAME', 'pgroonga_language_model_vectorize'
	LANGUAGE C
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_similar_text_condition
	(query text, condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_similar_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 10000;

CREATE OPERATOR &@* (
	PROCEDURE = pgroonga_similar_text_condition,
	LEFTARG = text,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_similar_distance_text_condition
	(query text, condition pgroonga_condition)
	RETURNS float8
	AS 'MODULE_PATHNAME', 'pgroonga_similar_distance_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 10000;

CREATE OPERATOR <&@*> (
	PROCEDURE = pgroonga_similar_distance_text_condition,
	LEFTARG = text,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR CLASS pgroonga_text_semantic_search_ops_v2 FOR TYPE text
	USING pgroonga AS
		OPERATOR 29 &@*,
		OPERATOR 48 &@* (text, pgroonga_condition),
		OPERATOR 49 <&@*> (text, pgroonga_condition) FOR ORDER BY float_ops;
