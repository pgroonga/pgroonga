-- Update amstrategies for old PostgreSQL
DO LANGUAGE plpgsql $$
BEGIN
	UPDATE pg_am SET amstrategies = 36
	 WHERE amname = 'pgroonga';
EXCEPTION
	WHEN undefined_column THEN -- Ignore
END;
$$;

CREATE FUNCTION pgroonga_regexp_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_regexp_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &~| (
	PROCEDURE = pgroonga_regexp_in_text,
	LEFTARG = text,
	RIGHTARG = text[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_regexp_in_varchar(varchar, varchar[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_regexp_in_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &~| (
	PROCEDURE = pgroonga_regexp_in_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

ALTER OPERATOR FAMILY pgroonga_text_regexp_ops_v2 USING pgroonga
	ADD
		OPERATOR 35 &~| (text, text[]);

ALTER OPERATOR FAMILY pgroonga_varchar_regexp_ops_v2 USING pgroonga
	ADD
		OPERATOR 35 &~| (varchar, varchar[]);


CREATE FUNCTION pgroonga_not_prefix_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_not_prefix_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR !&^| (
	PROCEDURE = pgroonga_not_prefix_in_text,
	LEFTARG = text,
	RIGHTARG = text[],
	NEGATOR = &^|,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

ALTER OPERATOR FAMILY pgroonga_text_term_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 36 !&^| (text, text[]);

ALTER OPERATOR FAMILY pgroonga_text_term_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 11 @> (anyarray, anyarray);

ALTER OPERATOR FAMILY pgroonga_varchar_term_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 11 @> (anyarray, anyarray);
