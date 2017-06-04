-- Update amstrategies for old PostgreSQL
DO LANGUAGE plpgsql $$
BEGIN
	SELECT amstrategies FROM pg_am LIMIT 0;
EXCEPTION
	WHEN syntax_error THEN
		UPDATE pg_am SET amstrategies = 27
		 WHERE amname = 'pgroonga';
END;
$$;

-- Update pgroonga.text_full_text_search_ops_v2
DROP OPERATOR CLASS pgroonga.text_full_text_search_ops_v2 USING pgroonga;
DROP OPERATOR &@> (text, text[]);
DROP OPERATOR &?> (text, text[]);
DROP FUNCTION pgroonga.match_contain_text(text, text[]);
DROP FUNCTION pgroonga.query_contain_text(text, text[]);

CREATE FUNCTION pgroonga.match_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

/* Deprecated since 1.2.1. */
CREATE OPERATOR &@> (
	PROCEDURE = pgroonga.match_in_text,
	LEFTARG = text,
	RIGHTARG = text[]
);

CREATE OPERATOR &@| (
	PROCEDURE = pgroonga.match_in_text,
	LEFTARG = text,
	RIGHTARG = text[]
);

CREATE FUNCTION pgroonga.query_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

/* Deprecated since 1.2.1. */
CREATE OPERATOR &?> (
	PROCEDURE = pgroonga.query_in_text,
	LEFTARG = text,
	RIGHTARG = text[]
);

CREATE OPERATOR &?| (
	PROCEDURE = pgroonga.query_in_text,
	LEFTARG = text,
	RIGHTARG = text[]
);

CREATE OPERATOR CLASS pgroonga.text_full_text_search_ops_v2 FOR TYPE text
	USING pgroonga AS
		OPERATOR 6 ~~,
		OPERATOR 7 ~~*,
		OPERATOR 8 %%, -- For backward compatibility
		OPERATOR 9 @@, -- For backward compatibility
		OPERATOR 12 &@,
		OPERATOR 13 &?,
		OPERATOR 14 &~?,
		OPERATOR 15 &`,
		OPERATOR 18 &@| (text, text[]),
		OPERATOR 19 &?| (text, text[]),
		OPERATOR 26 &@> (text, text[]), -- For backward compatibility
		OPERATOR 27 &?> (text, text[]); -- For backward compatibility

-- Add pgroonga.text_array_full_text_search_ops_v2
CREATE FUNCTION pgroonga.match_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga.match_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.query_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &? (
	PROCEDURE = pgroonga.query_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.similar_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_similar_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &~? (
	PROCEDURE = pgroonga.similar_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.script_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_script_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &` (
	PROCEDURE = pgroonga.script_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.match_in_text_array(text[], text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_in_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@| (
	PROCEDURE = pgroonga.match_in_text_array,
	LEFTARG = text[],
	RIGHTARG = text[]
);

CREATE FUNCTION pgroonga.query_in_text_array(text[], text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_in_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &?| (
	PROCEDURE = pgroonga.query_in_text_array,
	LEFTARG = text[],
	RIGHTARG = text[]
);

CREATE OPERATOR CLASS pgroonga.text_array_full_text_search_ops_v2
	FOR TYPE text[]
	USING pgroonga AS
		OPERATOR 8 %% (text[], text), -- For backward compatibility
		OPERATOR 9 @@ (text[], text), -- For backward compatibility
		OPERATOR 12 &@ (text[], text),
		OPERATOR 13 &? (text[], text),
		OPERATOR 14 &~? (text[], text),
		OPERATOR 15 &` (text[], text),
		OPERATOR 18 &@| (text[], text[]),
		OPERATOR 19 &?| (text[], text[]);

-- Add pgroonga.varchar_full_text_search_ops_v2
CREATE FUNCTION pgroonga.match_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga.match_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar
);

CREATE FUNCTION pgroonga.query_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &? (
	PROCEDURE = pgroonga.query_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar
);

CREATE FUNCTION pgroonga.similar_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_similar_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &~? (
	PROCEDURE = pgroonga.similar_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar
);

CREATE FUNCTION pgroonga.script_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_script_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &` (
	PROCEDURE = pgroonga.script_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar
);

CREATE FUNCTION pgroonga.match_in_varchar(varchar, varchar[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_in_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &@| (
	PROCEDURE = pgroonga.match_in_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar[]
);

CREATE FUNCTION pgroonga.query_in_varchar(varchar, varchar[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_in_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &?| (
	PROCEDURE = pgroonga.query_in_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar[]
);

CREATE OPERATOR CLASS pgroonga.varchar_full_text_search_ops_v2
	FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 8 %%, -- For backward compatibility
		OPERATOR 9 @@, -- For backward compatibility
		OPERATOR 12 &@,
		OPERATOR 13 &?,
		OPERATOR 14 &~?,
		OPERATOR 15 &`,
		OPERATOR 18 &@| (varchar, varchar[]),
		OPERATOR 19 &?| (varchar, varchar[]);

-- Add v2 compatible operators to full text search ops for varchar
ALTER OPERATOR FAMILY pgroonga.varchar_full_text_search_ops USING pgroonga
	ADD
		OPERATOR 12 &@ (varchar, varchar),
		OPERATOR 13 &? (varchar, varchar);

-- Add &^| and &^~| to pgroonga.text_term_search_ops_v2.
-- Add &^ and &^~ to pgroonga.text_array_term_search_ops_v2.
-- &^> and &^~> are alias of &^ and &^~.
DROP OPERATOR CLASS pgroonga.text_array_term_search_ops_v2 USING pgroonga;
DROP OPERATOR &^> (text[], text);
DROP OPERATOR &^~> (text[], text);
DROP FUNCTION pgroonga.prefix_contain_text_array(text[], text);
DROP FUNCTION pgroonga.prefix_rk_contain_text_array(text[], text);

CREATE FUNCTION pgroonga.prefix_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga.prefix_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

/* Deprecated since 1.2.1. */
CREATE OPERATOR &^> (
	PROCEDURE = pgroonga.prefix_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.prefix_rk_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &^~ (
	PROCEDURE = pgroonga.prefix_rk_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

/* Deprecated since 1.2.1. */
CREATE OPERATOR &^~> (
	PROCEDURE = pgroonga.prefix_rk_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.prefix_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &^| (
	PROCEDURE = pgroonga.prefix_in_text,
	LEFTARG = text,
	RIGHTARG = text[]
);

CREATE FUNCTION pgroonga.prefix_in_text_array(text[], text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_in_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &^| (
	PROCEDURE = pgroonga.prefix_in_text_array,
	LEFTARG = text[],
	RIGHTARG = text[]
);

CREATE FUNCTION pgroonga.prefix_rk_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &^~| (
	PROCEDURE = pgroonga.prefix_rk_in_text,
	LEFTARG = text,
	RIGHTARG = text[]
);

CREATE FUNCTION pgroonga.prefix_rk_in_text_array(text[], text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_in_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &^~| (
	PROCEDURE = pgroonga.prefix_rk_in_text_array,
	LEFTARG = text[],
	RIGHTARG = text[]
);

ALTER OPERATOR FAMILY pgroonga.text_term_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 20 &^| (text, text[]),
		OPERATOR 21 &^~| (text, text[]);

CREATE OPERATOR CLASS pgroonga.text_array_term_search_ops_v2 FOR TYPE text[]
	USING pgroonga AS
		OPERATOR 16 &^ (text[], text),
		OPERATOR 17 &^~ (text[], text),
		OPERATOR 20 &^| (text[], text[]),
		OPERATOR 21 &^~| (text[], text[]),
		OPERATOR 24 &^> (text[], text), -- For backward compatibility
		OPERATOR 25 &^~> (text[], text); -- For backward compatibility

-- Add pgroonga.text_regexp_ops_v2.
-- Add pgroonga.varchar_regexp_ops_v2.
CREATE FUNCTION pgroonga.regexp_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_regexp_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &~ (
	PROCEDURE = pgroonga.regexp_text,
	LEFTARG = text,
	RIGHTARG = text
);

CREATE FUNCTION pgroonga.regexp_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_regexp_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &~ (
	PROCEDURE = pgroonga.regexp_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar
);

CREATE OPERATOR CLASS pgroonga.text_regexp_ops_v2 FOR TYPE text
	USING pgroonga AS
		OPERATOR 6 ~~,
		OPERATOR 7 ~~*,
		OPERATOR 10 @~,
		OPERATOR 22 &~;

CREATE OPERATOR CLASS pgroonga.varchar_regexp_ops_v2 FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 10 @~,
		OPERATOR 22 &~;

-- Add pgroonga.varchar_array_term_search_ops_v2.
CREATE FUNCTION pgroonga.contain_varchar_array(varchar[], varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_contain_varchar_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &> (
	PROCEDURE = pgroonga.contain_varchar_array,
	LEFTARG = varchar[],
	RIGHTARG = varchar
);

CREATE OPERATOR CLASS pgroonga.varchar_array_term_search_ops_v2
	FOR TYPE varchar[]
	USING pgroonga AS
		OPERATOR 8 %% (varchar[], varchar), -- For backward compatibility
		OPERATOR 23 &> (varchar[], varchar);

-- Add v2 compatible operators to full text search ops for varchar
ALTER OPERATOR FAMILY pgroonga.varchar_array_ops USING pgroonga
	ADD
		OPERATOR 23 &> (varchar[], varchar);

-- Rename "pgroonga_match_jsonb" to "pgroonga_match_script_jsonb"
DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND THEN
		DROP OPERATOR CLASS pgroonga.jsonb_ops USING pgroonga;
		DROP OPERATOR @@ (jsonb, text);
		DROP FUNCTION pgroonga.match_query(jsonb, text);

		CREATE FUNCTION pgroonga.match_script_jsonb(jsonb, text)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_match_script_jsonb'
			LANGUAGE C
			IMMUTABLE
			STRICT;

		CREATE OPERATOR @@ (
			PROCEDURE = pgroonga.match_script_jsonb,
			LEFTARG = jsonb,
			RIGHTARG = text
		);

		CREATE OPERATOR CLASS pgroonga.jsonb_ops DEFAULT FOR TYPE jsonb
			USING pgroonga AS
				OPERATOR 9 @@ (jsonb, text),
				OPERATOR 11 @>;
	END IF;
END;
$$;

-- Add pgroonga.jsonb_ops_v2
DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND THEN
		CREATE FUNCTION pgroonga.match_jsonb(jsonb, text)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_match_jsonb'
			LANGUAGE C
			IMMUTABLE
			STRICT;

		CREATE OPERATOR &@ (
			PROCEDURE = pgroonga.match_jsonb,
			LEFTARG = jsonb,
			RIGHTARG = text
		);

		CREATE FUNCTION pgroonga.query_jsonb(jsonb, text)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_query_jsonb'
			LANGUAGE C
			IMMUTABLE
			STRICT;

		CREATE OPERATOR &? (
			PROCEDURE = pgroonga.query_jsonb,
			LEFTARG = jsonb,
			RIGHTARG = text
		);

		CREATE FUNCTION pgroonga.script_jsonb(jsonb, text)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_script_jsonb'
			LANGUAGE C
			IMMUTABLE
			STRICT;

		CREATE OPERATOR &` (
			PROCEDURE = pgroonga.script_jsonb,
			LEFTARG = jsonb,
			RIGHTARG = text
		);
		CREATE OPERATOR CLASS pgroonga.jsonb_ops_v2
			FOR TYPE jsonb
			USING pgroonga AS
				OPERATOR 9 @@ (jsonb, text), -- For backward compatibility
				OPERATOR 11 @>,
				OPERATOR 12 &@ (jsonb, text),
				OPERATOR 13 &? (jsonb, text),
				OPERATOR 15 &` (jsonb, text);
	END IF;
END;
$$;

-- Add v2 compatible operators to ops for jsonb
DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND THEN
		ALTER OPERATOR FAMILY pgroonga.jsonb_ops USING pgroonga
			ADD
				OPERATOR 12 &@ (jsonb, text),
				OPERATOR 13 &? (jsonb, text),
				OPERATOR 15 &` (jsonb, text);
	END IF;
END;
$$;

-- Add v2 compatible operators to regexp ops for text and varchar
ALTER OPERATOR FAMILY pgroonga.text_regexp_ops USING pgroonga
	ADD
		OPERATOR 22 &~ (text, text);
ALTER OPERATOR FAMILY pgroonga.varchar_regexp_ops USING pgroonga
	ADD
		OPERATOR 22 &~ (varchar, varchar);
