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
