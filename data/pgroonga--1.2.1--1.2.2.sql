CREATE FUNCTION pgroonga.query_expand(tableName cstring,
				      termColumnName text,
				      synonymsColumnName text,
				      query text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_query_expand'
	LANGUAGE C
	STABLE
	STRICT;

ALTER OPERATOR FAMILY pgroonga.text_term_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 1 < (text, text),
		OPERATOR 2 <= (text, text),
		OPERATOR 3 = (text, text),
		OPERATOR 4 >= (text, text),
		OPERATOR 5 > (text, text);

-- Update amstrategies for old PostgreSQL
DO LANGUAGE plpgsql $$
BEGIN
	UPDATE pg_am SET amstrategies = 30
	 WHERE amname = 'pgroonga';
EXCEPTION
	WHEN undefined_column THEN -- Ignore
END;
$$;

-- &? -> &@~
-- &~? -> &@*
-- &?| -> &@~|
CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga.query_text,
	LEFTARG = text,
	RIGHTARG = text
);

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga.query_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga.query_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar
);

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga.query_jsonb,
	LEFTARG = jsonb,
	RIGHTARG = text
);

CREATE OPERATOR &@* (
	PROCEDURE = pgroonga.similar_text,
	LEFTARG = text,
	RIGHTARG = text
);

CREATE OPERATOR &@* (
	PROCEDURE = pgroonga.similar_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

CREATE OPERATOR &@* (
	PROCEDURE = pgroonga.similar_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar
);

CREATE OPERATOR &@~| (
	PROCEDURE = pgroonga.query_in_text,
	LEFTARG = text,
	RIGHTARG = text[]
);

CREATE OPERATOR &@~| (
	PROCEDURE = pgroonga.query_in_text_array,
	LEFTARG = text[],
	RIGHTARG = text[]
);

CREATE OPERATOR &@~| (
	PROCEDURE = pgroonga.query_in_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar[]
);

ALTER OPERATOR FAMILY pgroonga.text_full_text_search_ops USING pgroonga
	ADD
		OPERATOR 28 &@~ (text, text);

ALTER OPERATOR FAMILY pgroonga.text_array_full_text_search_ops USING pgroonga
	ADD
		OPERATOR 28 &@~ (text[], text);

ALTER OPERATOR FAMILY pgroonga.varchar_full_text_search_ops USING pgroonga
	ADD
		OPERATOR 28 &@~ (varchar, varchar);

ALTER OPERATOR FAMILY pgroonga.jsonb_ops USING pgroonga
	ADD
		OPERATOR 28 &@~ (jsonb, text);

ALTER OPERATOR FAMILY pgroonga.text_full_text_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 28 &@~ (text, text),
		OPERATOR 29 &@* (text, text),
		OPERATOR 30 &@~| (text, text[]);

ALTER OPERATOR FAMILY pgroonga.text_array_full_text_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 28 &@~ (text[], text),
		OPERATOR 29 &@* (text[], text),
		OPERATOR 30 &@~| (text[], text[]);

ALTER OPERATOR FAMILY pgroonga.varchar_full_text_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 28 &@~ (varchar, varchar),
		OPERATOR 29 &@* (varchar, varchar),
		OPERATOR 30 &@~| (varchar, varchar[]);

ALTER OPERATOR FAMILY pgroonga.jsonb_ops_v2 USING pgroonga
	ADD
		OPERATOR 28 &@~ (jsonb, text);
