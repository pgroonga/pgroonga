-- Upgrade SQL

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		DROP OPERATOR public.%% (text, text);
		DROP OPERATOR public.%% (text[], text);
		DROP OPERATOR public.%% (varchar, varchar);
		DROP OPERATOR public.%% (varchar[], varchar);

		DROP OPERATOR public.@@ (text, text);
		DROP OPERATOR public.@@ (text[], text);
		DROP OPERATOR public.@@ (varchar, varchar);

		DROP OPERATOR public.@~ (text, text);
		DROP OPERATOR public.@~ (varchar, varchar);

		DROP OPERATOR public.&@ (text, text);
		DROP OPERATOR public.&@ (text[], text);
		DROP OPERATOR public.&@ (varchar, varchar);
		DROP OPERATOR public.&@ (jsonb, text);

		DROP OPERATOR public.&? (text, text);
		DROP OPERATOR public.&@~ (text, text);

		DROP OPERATOR public.&? (text[], text);
		DROP OPERATOR public.&@~ (text[], text);

		DROP OPERATOR public.&? (varchar, varchar);
		DROP OPERATOR public.&@~ (varchar, varchar);

		DROP OPERATOR public.&? (jsonb, text);
		DROP OPERATOR public.&@~ (jsonb, text);

		DROP OPERATOR public.&~? (text, text);
		DROP OPERATOR public.&@* (text, text);

		DROP OPERATOR public.&~? (text[], text);
		DROP OPERATOR public.&@* (text[], text);

		DROP OPERATOR public.&~? (varchar, varchar);
		DROP OPERATOR public.&@* (varchar, varchar);

		DROP OPERATOR public.&^ (text, text);

		DROP OPERATOR public.&^ (text[], text);
		DROP OPERATOR public.&^> (text[], text);

		DROP OPERATOR public.&^~ (text, text);

		DROP OPERATOR public.&^~ (text[], text);
		DROP OPERATOR public.&^~> (text[], text);

		DROP OPERATOR public.&` (text, text);
		DROP OPERATOR public.&` (text[], text);
		DROP OPERATOR public.&` (varchar, varchar);
		DROP OPERATOR public.&` (jsonb, text);

		DROP OPERATOR public.&@> (text, text[]);

		DROP OPERATOR public.&@| (text, text[]);
		DROP OPERATOR public.&@| (text[], text[]);
		DROP OPERATOR public.&@| (varchar, varchar[]);

		DROP OPERATOR public.&?> (text, text[]);
		DROP OPERATOR public.&?| (text, text[]);
		DROP OPERATOR public.&@~| (text, text[]);

		DROP OPERATOR public.&?| (text[], text[]);
		DROP OPERATOR public.&@~| (text[], text[]);

		DROP OPERATOR public.&?| (varchar, varchar[]);
		DROP OPERATOR public.&@~| (varchar, varchar[]);

		DROP OPERATOR public.&^| (text, text[]);
		DROP OPERATOR public.&^| (text[], text[]);

		DROP OPERATOR public.&^~| (text, text[]);
		DROP OPERATOR public.&^~| (text[], text[]);

		DROP OPERATOR public.&~ (text, text);
		DROP OPERATOR public.&~ (varchar, varchar);

		DROP OPERATOR public.@@ (jsonb, text);
	END IF;
END;
$$;


CREATE FUNCTION pgroonga_query_extract_keywords(query text,
						index_name text DEFAULT '')
	RETURNS text[]
	AS 'MODULE_PATHNAME', 'pgroonga_query_extract_keywords'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;
DROP FUNCTION pgroonga_query_extract_keywords(query text);


CREATE FUNCTION pgroonga_equal_query_text_array(targets text[], query text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_query_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &=~ (
	PROCEDURE = pgroonga_equal_query_text_array,
	LEFTARG = text[],
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

ALTER OPERATOR FAMILY pgroonga_text_array_term_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 40 &=~ (text[], text);


CREATE FUNCTION pgroonga_equal_query_varchar_array(targets varchar[], query text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_query_varchar_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &=~ (
	PROCEDURE = pgroonga_equal_query_varchar_array,
	LEFTARG = varchar[],
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

ALTER OPERATOR FAMILY pgroonga_varchar_array_term_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 40 &=~ (varchar[], text);
