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
