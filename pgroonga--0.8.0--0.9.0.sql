UPDATE pg_catalog.pg_am SET amstrategies = 9 WHERE amname = 'pgroonga';

DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND
	THEN
		CREATE FUNCTION pgroonga.match(jsonb, text)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_match_jsonb'
			LANGUAGE C
			IMMUTABLE
			STRICT;

		CREATE OPERATOR @@ (
			PROCEDURE = pgroonga.match,
			LEFTARG = jsonb,
			RIGHTARG = text
		);

		CREATE OPERATOR CLASS pgroonga.jsonb_ops DEFAULT FOR TYPE jsonb
			USING pgroonga AS
				OPERATOR 8 @@ (jsonb, text),
				OPERATOR 9 @>;
	END IF;
END;
$$;
