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
