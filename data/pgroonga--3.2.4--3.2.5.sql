-- Upgrade SQL

CREATE OPERATOR CLASS pgroonga_text_array_regexp_ops_v2 FOR TYPE text[]
	USING pgroonga AS
		OPERATOR 22 &~ (text[], text);

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
