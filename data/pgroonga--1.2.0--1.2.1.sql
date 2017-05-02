-- Add v1 compatible operators to full text search ops for text
ALTER OPERATOR FAMILY pgroonga.text_full_text_search_ops_v2 USING pgroonga
	ADD
		OPERATOR 8 %% (text, text),
		OPERATOR 9 @@ (text, text);

-- Remove "_contain" from function names of &^> and &^~>.
DROP OPERATOR CLASS pgroonga.text_array_term_search_ops_v2 USING pgroonga;

DROP OPERATOR &^> (text[], text);

DROP FUNCTION pgroonga.prefix_contain_text_array(text[], text);

CREATE FUNCTION pgroonga.prefix_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &^> (
	PROCEDURE = pgroonga.prefix_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

DROP OPERATOR &^~> (text[], text);

DROP FUNCTION pgroonga.prefix_rk_contain_text_array(text[], text);

CREATE FUNCTION pgroonga.prefix_rk_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR &^~> (
	PROCEDURE = pgroonga.prefix_rk_text_array,
	LEFTARG = text[],
	RIGHTARG = text
);

CREATE OPERATOR CLASS pgroonga.text_array_term_search_ops_v2 FOR TYPE text[]
	USING pgroonga AS
		OPERATOR 20 &^> (text[], text),
		OPERATOR 21 &^~> (text[], text);

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

CREATE OPERATOR &@> (
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

CREATE OPERATOR &?> (
	PROCEDURE = pgroonga.query_in_text_array,
	LEFTARG = text[],
	RIGHTARG = text[]
);

CREATE OPERATOR CLASS pgroonga.text_array_full_text_search_ops_v2
	FOR TYPE text[]
	USING pgroonga AS
		OPERATOR 12 &@ (text[], text),
		OPERATOR 13 &? (text[], text),
		OPERATOR 14 &~? (text[], text),
		OPERATOR 15 &` (text[], text),
		OPERATOR 18 &@> (text[], text[]),
		OPERATOR 19 &?> (text[], text[]);
