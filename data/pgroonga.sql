CREATE TYPE pgroonga_full_text_search_condition AS (
  query text,
  weigths int[],
  indexName text
);

CREATE TYPE pgroonga_full_text_search_condition_with_scorers AS (
  query text,
  weigths int[],
  scorers text[],
  indexName text
);

CREATE FUNCTION pgroonga_score("row" record)
	RETURNS float8
	AS 'MODULE_PATHNAME', 'pgroonga_score_row'
	LANGUAGE C
	VOLATILE
	STRICT;

CREATE FUNCTION pgroonga_score(tableoid oid, ctid tid)
	RETURNS float8
	AS 'MODULE_PATHNAME', 'pgroonga_score_ctid'
	LANGUAGE C
	VOLATILE
	STRICT;

CREATE FUNCTION pgroonga_table_name(indexName cstring)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_table_name'
	LANGUAGE C
	STABLE
	STRICT;

CREATE FUNCTION pgroonga_command(groongaCommand text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_command'
	LANGUAGE C
	VOLATILE
	STRICT;

CREATE FUNCTION pgroonga_command(groongaCommand text, arguments text[])
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_command'
	LANGUAGE C
	VOLATILE
	STRICT;

CREATE FUNCTION pgroonga_query_expand(tableName cstring,
				      termColumnName text,
				      synonymsColumnName text,
				      query text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_query_expand'
	LANGUAGE C
	STABLE
	STRICT;

CREATE FUNCTION pgroonga_snippet_html(target text, keywords text[])
	RETURNS text[]
	AS 'MODULE_PATHNAME', 'pgroonga_snippet_html'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_highlight_html(target text, keywords text[])
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_highlight_html'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_highlight_html(target text,
				        keywords text[],
				        indexName cstring)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_highlight_html'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_match_positions_byte(target text, keywords text[])
	RETURNS integer[2][]
	AS 'MODULE_PATHNAME', 'pgroonga_match_positions_byte'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_match_positions_character(target text, keywords text[])
	RETURNS integer[2][]
	AS 'MODULE_PATHNAME', 'pgroonga_match_positions_character'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_query_extract_keywords(query text)
	RETURNS text[]
	AS 'MODULE_PATHNAME', 'pgroonga_query_extract_keywords'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_flush(indexName cstring)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_flush'
	LANGUAGE C
	VOLATILE
	STRICT;

CREATE FUNCTION pgroonga_command_escape_value(value text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_command_escape_value'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_query_escape(query text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_query_escape'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_escape(value text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_string'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_escape(value text, special_characters text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_string'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_escape(value boolean)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_boolean'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_escape(value int2)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_int2'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_escape(value int4)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_int4'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_escape(value int8)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_int8'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_escape(value float4)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_float4'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_escape(value float8)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_float8'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_escape(value timestamp)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_timestamptz'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_escape(value timestamptz)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_timestamptz'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_wal_apply()
	RETURNS bigint
	AS 'MODULE_PATHNAME', 'pgroonga_wal_apply_all'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_wal_apply(indexName cstring)
	RETURNS bigint
	AS 'MODULE_PATHNAME', 'pgroonga_wal_apply_index'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_wal_truncate()
	RETURNS bigint
	AS 'MODULE_PATHNAME', 'pgroonga_wal_truncate_all'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_wal_truncate(indexName cstring)
	RETURNS bigint
	AS 'MODULE_PATHNAME', 'pgroonga_wal_truncate_index'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_is_writable()
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_is_writable'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_set_writable(newWritable bool)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_set_writable'
	LANGUAGE C
	VOLATILE
	STRICT;

CREATE FUNCTION pgroonga_normalize(target text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_normalize'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_normalize(target text, normalizerName text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_normalize'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_tokenize(target text, VARIADIC options text[])
	RETURNS json[]
	AS 'MODULE_PATHNAME', 'pgroonga_tokenize'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_vacuum()
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_vacuum'
	LANGUAGE C
	VOLATILE
	STRICT;


/* v1 */
CREATE FUNCTION pgroonga_match_term(target text, term text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_term_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_match_term(target text[], term text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_term_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_match_term(target varchar, term varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_term_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_match_term(target varchar[], term varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_term_varchar_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR %% (
	PROCEDURE = pgroonga_match_term,
	LEFTARG = text,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR %% (
	PROCEDURE = pgroonga_match_term,
	LEFTARG = text[],
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR %% (
	PROCEDURE = pgroonga_match_term,
	LEFTARG = varchar,
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR %% (
	PROCEDURE = pgroonga_match_term,
	LEFTARG = varchar[],
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);


CREATE FUNCTION pgroonga_match_query(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_query_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_match_query(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_query_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_match_query(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_query_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR @@ (
	PROCEDURE = pgroonga_match_query,
	LEFTARG = text,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR @@ (
	PROCEDURE = pgroonga_match_query,
	LEFTARG = text[],
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR @@ (
	PROCEDURE = pgroonga_match_query,
	LEFTARG = varchar,
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);


CREATE FUNCTION pgroonga_match_regexp(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_regexp_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga_match_regexp(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_regexp_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE OPERATOR @~ (
	PROCEDURE = pgroonga_match_regexp,
	LEFTARG = text,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR @~ (
	PROCEDURE = pgroonga_match_regexp,
	LEFTARG = varchar,
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);


/* v2 */
CREATE FUNCTION pgroonga_match_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_text,
	LEFTARG = text,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_text_condition
	(target text, condition pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_text_condition,
	LEFTARG = text,
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_text_condition_with_scorers
	(target text,
	 condition pgroonga_full_text_search_condition_with_scorers)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text_condition_with_scorers'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_text_condition_with_scorers,
	LEFTARG = text,
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_text_array,
	LEFTARG = text[],
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_text_array_condition
	(target text[], condition pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_text_array_condition,
	LEFTARG = text[],
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_text_array_condition_with_scorers
	(target text[],
	 condition pgroonga_full_text_search_condition_with_scorers)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text_array_condition_with_scorers'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_text_array_condition_with_scorers,
	LEFTARG = text[],
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_varchar_condition
	(target varchar, condition pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_varchar_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_varchar_condition,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_varchar_condition_with_scorers
	(target varchar,
	 condition pgroonga_full_text_search_condition_with_scorers)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_varchar_condition_with_scorers'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_varchar_condition_with_scorers,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_contain_varchar_array(varchar[], varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_contain_varchar_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &> (
	PROCEDURE = pgroonga_contain_varchar_array,
	LEFTARG = varchar[],
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND THEN
		CREATE FUNCTION pgroonga_match_jsonb(jsonb, text)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_match_jsonb'
			LANGUAGE C
			IMMUTABLE
			STRICT;

		CREATE OPERATOR &@ (
			PROCEDURE = pgroonga_match_jsonb,
			LEFTARG = jsonb,
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga_query_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

-- Deprecated since 1.2.2.
CREATE OPERATOR &? (
	PROCEDURE = pgroonga_query_text,
	LEFTARG = text,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_text,
	LEFTARG = text,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_text_condition
	(target text, condition pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_text_condition,
	LEFTARG = text,
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_text_condition_with_scorers
	(target text,
	 condition pgroonga_full_text_search_condition_with_scorers)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text_condition_with_scorers'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_text_condition_with_scorers,
	LEFTARG = text,
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

-- Deprecated since 1.2.2.
CREATE OPERATOR &? (
	PROCEDURE = pgroonga_query_text_array,
	LEFTARG = text[],
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_text_array,
	LEFTARG = text[],
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_text_array_condition
	(targets text[], condition pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_text_array_condition,
	LEFTARG = text[],
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_text_array_condition_with_scorers
	(targets text[],
	 condition pgroonga_full_text_search_condition_with_scorers)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text_array_condition_with_scorers'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_text_array_condition_with_scorers,
	LEFTARG = text[],
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

-- Deprecated since 1.2.2.
CREATE OPERATOR &? (
	PROCEDURE = pgroonga_query_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_varchar_condition
	(target varchar, condition pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_varchar_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_varchar_condition,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_varchar_condition_with_scorers
	(target varchar,
	 condition pgroonga_full_text_search_condition_with_scorers)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_varchar_condition_with_scorers'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_varchar_condition_with_scorers,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND THEN
		CREATE FUNCTION pgroonga_query_jsonb(jsonb, text)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_query_jsonb'
			LANGUAGE C
			IMMUTABLE
			STRICT
			COST 200;

		-- Deprecated since 1.2.2.
		CREATE OPERATOR &? (
			PROCEDURE = pgroonga_query_jsonb,
			LEFTARG = jsonb,
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR &@~ (
			PROCEDURE = pgroonga_query_jsonb,
			LEFTARG = jsonb,
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga_similar_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_similar_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

-- Deprecated since 1.2.2.
CREATE OPERATOR &~? (
	PROCEDURE = pgroonga_similar_text,
	LEFTARG = text,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR &@* (
	PROCEDURE = pgroonga_similar_text,
	LEFTARG = text,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_similar_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_similar_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

-- Deprecated since 1.2.2.
CREATE OPERATOR &~? (
	PROCEDURE = pgroonga_similar_text_array,
	LEFTARG = text[],
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR &@* (
	PROCEDURE = pgroonga_similar_text_array,
	LEFTARG = text[],
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_similar_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_similar_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

-- Deprecated since 1.2.2.
CREATE OPERATOR &~? (
	PROCEDURE = pgroonga_similar_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR &@* (
	PROCEDURE = pgroonga_similar_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_text,
	LEFTARG = text,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_text_array,
	LEFTARG = text[],
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

/* Deprecated since 1.2.1. */
CREATE OPERATOR &^> (
	PROCEDURE = pgroonga_prefix_text_array,
	LEFTARG = text[],
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_varchar_array(varchar[], varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_varchar_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_varchar_array,
	LEFTARG = varchar[],
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

/* Deprecated since 1.2.1. */
CREATE OPERATOR &^> (
	PROCEDURE = pgroonga_prefix_varchar_array,
	LEFTARG = varchar[],
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_rk_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &^~ (
	PROCEDURE = pgroonga_prefix_rk_text,
	LEFTARG = text,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_rk_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &^~ (
	PROCEDURE = pgroonga_prefix_rk_text_array,
	LEFTARG = text[],
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

/* Deprecated since 1.2.1. */
CREATE OPERATOR &^~> (
	PROCEDURE = pgroonga_prefix_rk_text_array,
	LEFTARG = text[],
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_rk_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &^~ (
	PROCEDURE = pgroonga_prefix_rk_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_rk_varchar_array(varchar[], varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_varchar_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &^~ (
	PROCEDURE = pgroonga_prefix_rk_varchar_array,
	LEFTARG = varchar[],
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

/* Deprecated since 1.2.1. */
CREATE OPERATOR &^~> (
	PROCEDURE = pgroonga_prefix_rk_varchar_array,
	LEFTARG = varchar[],
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_script_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_script_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &` (
	PROCEDURE = pgroonga_script_text,
	LEFTARG = text,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_script_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_script_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &` (
	PROCEDURE = pgroonga_script_text_array,
	LEFTARG = text[],
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_script_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_script_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &` (
	PROCEDURE = pgroonga_script_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND THEN
		CREATE FUNCTION pgroonga_script_jsonb(jsonb, text)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_script_jsonb'
			LANGUAGE C
			IMMUTABLE
			STRICT
			COST 200;

		CREATE OPERATOR &` (
			PROCEDURE = pgroonga_script_jsonb,
			LEFTARG = jsonb,
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga_match_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

/* Deprecated since 1.2.1. */
CREATE OPERATOR &@> (
	PROCEDURE = pgroonga_match_in_text,
	LEFTARG = text,
	RIGHTARG = text[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR &@| (
	PROCEDURE = pgroonga_match_in_text,
	LEFTARG = text,
	RIGHTARG = text[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_in_text_array(text[], text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_in_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &@| (
	PROCEDURE = pgroonga_match_in_text_array,
	LEFTARG = text[],
	RIGHTARG = text[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_in_varchar(varchar, varchar[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_in_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &@| (
	PROCEDURE = pgroonga_match_in_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

/* Deprecated since 1.2.1. */
CREATE OPERATOR &?> (
	PROCEDURE = pgroonga_query_in_text,
	LEFTARG = text,
	RIGHTARG = text[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

-- Deprecated since 1.2.2.
CREATE OPERATOR &?| (
	PROCEDURE = pgroonga_query_in_text,
	LEFTARG = text,
	RIGHTARG = text[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR &@~| (
	PROCEDURE = pgroonga_query_in_text,
	LEFTARG = text,
	RIGHTARG = text[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_in_text_array(text[], text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_in_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

-- Deprecated since 1.2.2.
CREATE OPERATOR &?| (
	PROCEDURE = pgroonga_query_in_text_array,
	LEFTARG = text[],
	RIGHTARG = text[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR &@~| (
	PROCEDURE = pgroonga_query_in_text_array,
	LEFTARG = text[],
	RIGHTARG = text[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_in_varchar(varchar, varchar[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_in_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

-- Deprecated since 1.2.2.
CREATE OPERATOR &?| (
	PROCEDURE = pgroonga_query_in_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE OPERATOR &@~| (
	PROCEDURE = pgroonga_query_in_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &^| (
	PROCEDURE = pgroonga_prefix_in_text,
	LEFTARG = text,
	RIGHTARG = text[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

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

CREATE FUNCTION pgroonga_prefix_in_text_array(text[], text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_in_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &^| (
	PROCEDURE = pgroonga_prefix_in_text_array,
	LEFTARG = text[],
	RIGHTARG = text[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_in_varchar(varchar, varchar[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_in_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &^| (
	PROCEDURE = pgroonga_prefix_in_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_in_varchar_array(varchar[], varchar[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_in_varchar_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &^| (
	PROCEDURE = pgroonga_prefix_in_varchar_array,
	LEFTARG = varchar[],
	RIGHTARG = varchar[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_rk_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &^~| (
	PROCEDURE = pgroonga_prefix_rk_in_text,
	LEFTARG = text,
	RIGHTARG = text[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_rk_in_text_array(text[], text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_in_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &^~| (
	PROCEDURE = pgroonga_prefix_rk_in_text_array,
	LEFTARG = text[],
	RIGHTARG = text[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_rk_in_varchar(varchar, varchar[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_in_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &^~| (
	PROCEDURE = pgroonga_prefix_rk_in_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_rk_in_varchar_array(varchar[], varchar[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_in_varchar_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &^~| (
	PROCEDURE = pgroonga_prefix_rk_in_varchar_array,
	LEFTARG = varchar[],
	RIGHTARG = varchar[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_regexp_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_regexp_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &~ (
	PROCEDURE = pgroonga_regexp_text,
	LEFTARG = text,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_regexp_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_regexp_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	COST 200;

CREATE OPERATOR &~ (
	PROCEDURE = pgroonga_regexp_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

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

DO LANGUAGE plpgsql $$
BEGIN
	EXECUTE 'DROP ACCESS METHOD IF EXISTS pgroonga CASCADE';
	CREATE FUNCTION pgroonga_handler(internal)
		RETURNS index_am_handler
		AS 'MODULE_PATHNAME', 'pgroonga_handler'
		LANGUAGE C;
	EXECUTE 'CREATE ACCESS METHOD pgroonga ' ||
		'TYPE INDEX ' ||
		'HANDLER pgroonga_handler';
EXCEPTION
	WHEN syntax_error THEN
		CREATE FUNCTION pgroonga_insert(internal)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_insert'
			LANGUAGE C;
		CREATE FUNCTION pgroonga_beginscan(internal)
			RETURNS internal
			AS 'MODULE_PATHNAME', 'pgroonga_beginscan'
			LANGUAGE C;
		CREATE FUNCTION pgroonga_gettuple(internal)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_gettuple'
			LANGUAGE C;
		CREATE FUNCTION pgroonga_getbitmap(internal)
			RETURNS bigint
			AS 'MODULE_PATHNAME', 'pgroonga_getbitmap'
			LANGUAGE C;
		CREATE FUNCTION pgroonga_rescan(internal)
			RETURNS void
			AS 'MODULE_PATHNAME', 'pgroonga_rescan'
			LANGUAGE C;
		CREATE FUNCTION pgroonga_endscan(internal)
			RETURNS void
			AS 'MODULE_PATHNAME', 'pgroonga_endscan'
			LANGUAGE C;
		CREATE FUNCTION pgroonga_build(internal)
			RETURNS internal
			AS 'MODULE_PATHNAME', 'pgroonga_build'
			LANGUAGE C;
		CREATE FUNCTION pgroonga_buildempty(internal)
			RETURNS internal
			AS 'MODULE_PATHNAME', 'pgroonga_buildempty'
			LANGUAGE C;
		CREATE FUNCTION pgroonga_bulkdelete(internal)
			RETURNS internal
			AS 'MODULE_PATHNAME', 'pgroonga_bulkdelete'
			LANGUAGE C;
		CREATE FUNCTION pgroonga_vacuumcleanup(internal)
			RETURNS internal
			AS 'MODULE_PATHNAME', 'pgroonga_vacuumcleanup'
			LANGUAGE C;
		CREATE FUNCTION pgroonga_canreturn(internal)
			RETURNS internal
			AS 'MODULE_PATHNAME', 'pgroonga_canreturn'
			LANGUAGE C;
		CREATE FUNCTION pgroonga_costestimate(internal)
			RETURNS internal
			AS 'MODULE_PATHNAME', 'pgroonga_costestimate'
			LANGUAGE C;
		CREATE FUNCTION pgroonga_options(internal)
			RETURNS internal
			AS 'MODULE_PATHNAME', 'pgroonga_options'
			LANGUAGE C;

		DELETE FROM pg_am WHERE amname = 'pgroonga';
		INSERT INTO pg_am VALUES(
			'pgroonga',	-- amname
			36,		-- amstrategies
			0,		-- amsupport
			true,		-- amcanorder
			true,		-- amcanorderbyop
			true,		-- amcanbackward
			true,		-- amcanunique
			true,		-- amcanmulticol
			true,		-- amoptionalkey
			true,		-- amsearcharray
			false,		-- amsearchnulls
			false,		-- amstorage
			true,		-- amclusterable
			false,		-- ampredlocks
			0,		-- amkeytype
			'pgroonga_insert',	-- aminsert
			'pgroonga_beginscan',	-- ambeginscan
			'pgroonga_gettuple',	-- amgettuple
			'pgroonga_getbitmap',	-- amgetbitmap
			'pgroonga_rescan',	-- amrescan
			'pgroonga_endscan',	-- amendscan
			0,		-- ammarkpos,
			0,		-- amrestrpos,
			'pgroonga_build',	-- ambuild
			'pgroonga_buildempty',	-- ambuildempty
			'pgroonga_bulkdelete',	-- ambulkdelete
			'pgroonga_vacuumcleanup',	-- amvacuumcleanup
			'pgroonga_canreturn',		-- amcanreturn
			'pgroonga_costestimate',	-- amcostestimate
			'pgroonga_options'	-- amoptions
		);
END;
$$;


/* v1 */
CREATE OPERATOR CLASS pgroonga_text_full_text_search_ops FOR TYPE text
	USING pgroonga AS
		OPERATOR 6 ~~,
		OPERATOR 7 ~~*,
		OPERATOR 8 %%,
		OPERATOR 9 @@,
		OPERATOR 12 &@,
		OPERATOR 13 &?, -- For backward compatibility
		OPERATOR 28 &@~;

CREATE OPERATOR CLASS pgroonga_text_array_full_text_search_ops
	FOR TYPE text[]
	USING pgroonga AS
		OPERATOR 8 %% (text[], text),
		OPERATOR 9 @@ (text[], text),
		OPERATOR 12 &@ (text[], text),
		OPERATOR 13 &? (text[], text), -- For backward compatibility
		OPERATOR 28 &@~ (text[], text);

CREATE OPERATOR CLASS pgroonga_varchar_full_text_search_ops FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 8 %%,
		OPERATOR 9 @@,
		OPERATOR 12 &@,
		OPERATOR 13 &?, -- For backward compatibility
		OPERATOR 28 &@~;

CREATE OPERATOR CLASS pgroonga_varchar_ops FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 1 < (text, text),
		OPERATOR 2 <= (text, text),
		OPERATOR 3 = (text, text),
		OPERATOR 4 >= (text, text),
		OPERATOR 5 > (text, text);

CREATE OPERATOR CLASS pgroonga_varchar_array_ops
	FOR TYPE varchar[]
	USING pgroonga AS
		OPERATOR 8 %% (varchar[], varchar),
		OPERATOR 23 &> (varchar[], varchar);

CREATE OPERATOR CLASS pgroonga_bool_ops DEFAULT FOR TYPE bool
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga_int2_ops DEFAULT FOR TYPE int2
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga_int4_ops DEFAULT FOR TYPE int4
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga_int8_ops DEFAULT FOR TYPE int8
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga_float4_ops DEFAULT FOR TYPE float4
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga_float8_ops DEFAULT FOR TYPE float8
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga_timestamp_ops DEFAULT FOR TYPE timestamp
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga_timestamptz_ops DEFAULT FOR TYPE timestamptz
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND THEN
		CREATE FUNCTION pgroonga_match_script_jsonb(jsonb, text)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_match_script_jsonb'
			LANGUAGE C
			IMMUTABLE
			STRICT;

		CREATE OPERATOR @@ (
			PROCEDURE = pgroonga_match_script_jsonb,
			LEFTARG = jsonb,
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR CLASS pgroonga_jsonb_ops FOR TYPE jsonb
			USING pgroonga AS
				OPERATOR 9 @@ (jsonb, text),
				OPERATOR 11 @>,
				OPERATOR 12 &@ (jsonb, text),
				OPERATOR 13 &? (jsonb, text), -- For backward compatibility
				OPERATOR 15 &` (jsonb, text),
				OPERATOR 28 &@~ (jsonb, text);
	END IF;
END;
$$;

CREATE OPERATOR CLASS pgroonga_text_regexp_ops FOR TYPE text
	USING pgroonga AS
		OPERATOR 6 ~~,
		OPERATOR 7 ~~*,
		OPERATOR 10 @~,
		OPERATOR 22 &~;

CREATE OPERATOR CLASS pgroonga_varchar_regexp_ops FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 10 @~,
		OPERATOR 22 &~;

/* v2 */
CREATE OPERATOR CLASS pgroonga_int4_array_ops DEFAULT FOR TYPE int4[]
	USING pgroonga AS
		OPERATOR 3 = (anyarray, anyarray);

CREATE OPERATOR CLASS pgroonga_text_full_text_search_ops_v2
	DEFAULT FOR TYPE text
	USING pgroonga AS
		OPERATOR 6 ~~,
		OPERATOR 7 ~~*,
		OPERATOR 8 %%, -- For backward compatibility
		OPERATOR 9 @@, -- For backward compatibility
		OPERATOR 12 &@,
		OPERATOR 13 &?, -- For backward compatibility
		OPERATOR 14 &~?, -- For backward compatibility
		OPERATOR 15 &`,
		OPERATOR 18 &@| (text, text[]),
		OPERATOR 19 &?| (text, text[]), -- For backward compatibility
		OPERATOR 26 &@> (text, text[]), -- For backward compatibility
		OPERATOR 27 &?> (text, text[]), -- For backward compatibility
		OPERATOR 28 &@~,
		OPERATOR 29 &@*,
		OPERATOR 30 &@~| (text, text[]),
		OPERATOR 31 &@ (text, pgroonga_full_text_search_condition),
		OPERATOR 32 &@~ (text, pgroonga_full_text_search_condition),
		OPERATOR 33 &@ (text, pgroonga_full_text_search_condition_with_scorers),
		OPERATOR 34 &@~ (text, pgroonga_full_text_search_condition_with_scorers);

CREATE OPERATOR CLASS pgroonga_text_array_full_text_search_ops_v2
	DEFAULT FOR TYPE text[]
	USING pgroonga AS
		OPERATOR 8 %% (text[], text), -- For backward compatibility
		OPERATOR 9 @@ (text[], text), -- For backward compatibility
		OPERATOR 12 &@ (text[], text),
		OPERATOR 13 &? (text[], text), -- For backward compatibility
		OPERATOR 14 &~? (text[], text), -- For backward compatibility
		OPERATOR 15 &` (text[], text),
		OPERATOR 18 &@| (text[], text[]),
		OPERATOR 19 &?| (text[], text[]), -- For backward compatibility
		OPERATOR 28 &@~ (text[], text),
		OPERATOR 29 &@* (text[], text),
		OPERATOR 30 &@~| (text[], text[]),
		OPERATOR 31 &@ (text[], pgroonga_full_text_search_condition),
		OPERATOR 32 &@~ (text[], pgroonga_full_text_search_condition),
		OPERATOR 33 &@ (text[], pgroonga_full_text_search_condition_with_scorers),
		OPERATOR 34 &@~ (text[], pgroonga_full_text_search_condition_with_scorers);

CREATE OPERATOR CLASS pgroonga_text_term_search_ops_v2 FOR TYPE text
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >,
		OPERATOR 16 &^,
		OPERATOR 17 &^~,
		OPERATOR 20 &^| (text, text[]),
		OPERATOR 21 &^~| (text, text[]),
		OPERATOR 36 !&^| (text, text[]);

CREATE OPERATOR CLASS pgroonga_text_array_term_search_ops_v2 FOR TYPE text[]
	USING pgroonga AS
		OPERATOR 16 &^ (text[], text),
		OPERATOR 17 &^~ (text[], text),
		OPERATOR 20 &^| (text[], text[]),
		OPERATOR 21 &^~| (text[], text[]),
		OPERATOR 24 &^> (text[], text), -- For backward compatibility
		OPERATOR 25 &^~> (text[], text); -- For backward compatibility

CREATE OPERATOR CLASS pgroonga_text_regexp_ops_v2 FOR TYPE text
	USING pgroonga AS
		OPERATOR 6 ~~,
		OPERATOR 7 ~~*,
		OPERATOR 10 @~, -- For backward compatibility
		OPERATOR 22 &~,
		OPERATOR 35 &~| (text, text[]);

CREATE OPERATOR CLASS pgroonga_varchar_term_search_ops_v2
	DEFAULT FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 1 < (text, text),
		OPERATOR 2 <= (text, text),
		OPERATOR 3 = (text, text),
		OPERATOR 4 >= (text, text),
		OPERATOR 5 > (text, text),
		OPERATOR 16 &^,
		OPERATOR 17 &^~,
		OPERATOR 20 &^| (varchar, varchar[]),
		OPERATOR 21 &^~| (varchar, varchar[]);

CREATE OPERATOR CLASS pgroonga_varchar_full_text_search_ops_v2
	FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 8 %%, -- For backward compatibility
		OPERATOR 9 @@, -- For backward compatibility
		OPERATOR 12 &@,
		OPERATOR 13 &?, -- For backward compatibility
		OPERATOR 14 &~?, -- For backward compatibility
		OPERATOR 15 &`,
		OPERATOR 18 &@| (varchar, varchar[]),
		OPERATOR 19 &?| (varchar, varchar[]), -- For backward compatibility
		OPERATOR 28 &@~,
		OPERATOR 29 &@*,
		OPERATOR 30 &@~| (varchar, varchar[]),
		OPERATOR 31 &@ (varchar, pgroonga_full_text_search_condition),
		OPERATOR 32 &@~ (varchar, pgroonga_full_text_search_condition),
		OPERATOR 33 &@ (varchar, pgroonga_full_text_search_condition_with_scorers),
		OPERATOR 34 &@~ (varchar, pgroonga_full_text_search_condition_with_scorers);

CREATE OPERATOR CLASS pgroonga_varchar_array_term_search_ops_v2
	DEFAULT FOR TYPE varchar[]
	USING pgroonga AS
		OPERATOR 8 %% (varchar[], varchar), -- For backward compatibility
		OPERATOR 16 &^ (varchar[], varchar),
		OPERATOR 17 &^~ (varchar[], varchar),
		OPERATOR 20 &^| (varchar[], varchar[]),
		OPERATOR 21 &^~| (varchar[], varchar[]),
		OPERATOR 23 &> (varchar[], varchar),
		OPERATOR 24 &^> (varchar[], varchar), -- For backward compatibility
		OPERATOR 25 &^~> (varchar[], varchar); -- For backward compatibility

CREATE OPERATOR CLASS pgroonga_varchar_regexp_ops_v2 FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 10 @~, -- For backward compatibility
		OPERATOR 22 &~,
		OPERATOR 35 &~| (varchar, varchar[]);

DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND THEN
		CREATE OPERATOR CLASS pgroonga_jsonb_ops_v2
			DEFAULT FOR TYPE jsonb
			USING pgroonga AS
				OPERATOR 9 @@ (jsonb, text), -- For backward compatibility
				OPERATOR 11 @>,
				OPERATOR 12 &@ (jsonb, text),
				OPERATOR 13 &? (jsonb, text), -- For backward compatibility
				OPERATOR 15 &` (jsonb, text),
				OPERATOR 28 &@~ (jsonb, text);

		CREATE OPERATOR CLASS pgroonga_jsonb_full_text_search_ops_v2
			FOR TYPE jsonb
			USING pgroonga AS
				OPERATOR 12 &@ (jsonb, text),
				OPERATOR 28 &@~ (jsonb, text);
	END IF;
END;
$$;

-- For backward compatibility

CREATE SCHEMA pgroonga;

CREATE FUNCTION pgroonga.score("row" record)
	RETURNS float8
	AS 'MODULE_PATHNAME', 'pgroonga_score'
	LANGUAGE C
	VOLATILE
	STRICT;

CREATE FUNCTION pgroonga.table_name(indexName cstring)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_table_name'
	LANGUAGE C
	STABLE
	STRICT;

CREATE FUNCTION pgroonga.command(groongaCommand text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_command'
	LANGUAGE C
	VOLATILE
	STRICT;

CREATE FUNCTION pgroonga.command(groongaCommand text, arguments text[])
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_command'
	LANGUAGE C
	VOLATILE
	STRICT;

CREATE FUNCTION pgroonga.query_expand(tableName cstring,
				      termColumnName text,
				      synonymsColumnName text,
				      query text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_query_expand'
	LANGUAGE C
	STABLE
	STRICT;

CREATE FUNCTION pgroonga.snippet_html(target text, keywords text[])
	RETURNS text[]
	AS 'MODULE_PATHNAME', 'pgroonga_snippet_html'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.highlight_html(target text, keywords text[])
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_highlight_html'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.match_positions_byte(target text, keywords text[])
	RETURNS integer[2][]
	AS 'MODULE_PATHNAME', 'pgroonga_match_positions_byte'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.match_positions_character(target text, keywords text[])
	RETURNS integer[2][]
	AS 'MODULE_PATHNAME', 'pgroonga_match_positions_character'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.query_extract_keywords(query text)
	RETURNS text[]
	AS 'MODULE_PATHNAME', 'pgroonga_query_extract_keywords'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.flush(indexName cstring)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_flush'
	LANGUAGE C
	VOLATILE
	STRICT;

CREATE FUNCTION pgroonga.command_escape_value(value text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_command_escape_value'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.query_escape(query text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_query_escape'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_string'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value text, special_characters text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_string'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value boolean)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_boolean'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value int2)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_int2'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value int4)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_int4'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value int8)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_int8'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value float4)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_float8'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value float8)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_float8'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value timestamp)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_timestamptz'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.escape(value timestamptz)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_timestamptz'
	LANGUAGE C
	IMMUTABLE
	STRICT;


/* v1 */
CREATE FUNCTION pgroonga.match_term(target text, term text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_term_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.match_term(target text[], term text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_term_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.match_term(target varchar, term varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_term_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.match_term(target varchar[], term varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_term_varchar_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.%% (
			PROCEDURE = pgroonga.match_term,
			LEFTARG = text,
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR public.%% (
			PROCEDURE = pgroonga.match_term,
			LEFTARG = text[],
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR public.%% (
			PROCEDURE = pgroonga.match_term,
			LEFTARG = varchar,
			RIGHTARG = varchar,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR public.%% (
			PROCEDURE = pgroonga.match_term,
			LEFTARG = varchar[],
			RIGHTARG = varchar,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.match_query(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_query_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.match_query(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_query_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.match_query(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_query_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.@@ (
			PROCEDURE = pgroonga.match_query,
			LEFTARG = text,
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR public.@@ (
			PROCEDURE = pgroonga.match_query,
			LEFTARG = text[],
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR public.@@ (
			PROCEDURE = pgroonga.match_query,
			LEFTARG = varchar,
			RIGHTARG = varchar,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.match_regexp(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_regexp_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

CREATE FUNCTION pgroonga.match_regexp(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_regexp_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.@~ (
			PROCEDURE = pgroonga.match_regexp,
			LEFTARG = text,
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR public.@~ (
			PROCEDURE = pgroonga.match_regexp,
			LEFTARG = varchar,
			RIGHTARG = varchar,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

/* v2 */
CREATE FUNCTION pgroonga.match_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&@ (
			PROCEDURE = pgroonga.match_text,
			LEFTARG = text,
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.match_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&@ (
			PROCEDURE = pgroonga.match_text_array,
			LEFTARG = text[],
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.match_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&@ (
			PROCEDURE = pgroonga.match_varchar,
			LEFTARG = varchar,
			RIGHTARG = varchar,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.contain_varchar_array(varchar[], varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_contain_varchar_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&> (
			PROCEDURE = pgroonga.contain_varchar_array,
			LEFTARG = varchar[],
			RIGHTARG = varchar,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

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

		IF current_schema() <> 'public' THEN
			CREATE OPERATOR public.&@ (
				PROCEDURE = pgroonga.match_jsonb,
				LEFTARG = jsonb,
				RIGHTARG = text,
				RESTRICT = contsel,
				JOIN = contjoinsel
			);
		END IF;
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.query_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		-- Deprecated since 1.2.2.
		CREATE OPERATOR public.&? (
			PROCEDURE = pgroonga.query_text,
			LEFTARG = text,
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR public.&@~ (
			PROCEDURE = pgroonga.query_text,
			LEFTARG = text,
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.query_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		-- Deprecated since 1.2.2.
		CREATE OPERATOR public.&? (
			PROCEDURE = pgroonga.query_text_array,
			LEFTARG = text[],
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR public.&@~ (
			PROCEDURE = pgroonga.query_text_array,
			LEFTARG = text[],
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.query_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		-- Deprecated since 1.2.2.
		CREATE OPERATOR public.&? (
			PROCEDURE = pgroonga.query_varchar,
			LEFTARG = varchar,
			RIGHTARG = varchar,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR public.&@~ (
			PROCEDURE = pgroonga.query_varchar,
			LEFTARG = varchar,
			RIGHTARG = varchar,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND THEN
		CREATE FUNCTION pgroonga.query_jsonb(jsonb, text)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_query_jsonb'
			LANGUAGE C
			IMMUTABLE
			STRICT;

		IF current_schema() <> 'public' THEN
			-- Deprecated since 1.2.2.
			CREATE OPERATOR public.&? (
				PROCEDURE = pgroonga.query_jsonb,
				LEFTARG = jsonb,
				RIGHTARG = text,
				RESTRICT = contsel,
				JOIN = contjoinsel
			);

			CREATE OPERATOR public.&@~ (
				PROCEDURE = pgroonga.query_jsonb,
				LEFTARG = jsonb,
				RIGHTARG = text,
				RESTRICT = contsel,
				JOIN = contjoinsel
			);
		END IF;
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.similar_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_similar_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		-- Deprecated since 1.2.2.
		CREATE OPERATOR public.&~? (
			PROCEDURE = pgroonga.similar_text,
			LEFTARG = text,
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR public.&@* (
			PROCEDURE = pgroonga.similar_text,
			LEFTARG = text,
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.similar_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_similar_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		-- Deprecated since 1.2.2.
		CREATE OPERATOR public.&~? (
			PROCEDURE = pgroonga.similar_text_array,
			LEFTARG = text[],
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR public.&@* (
			PROCEDURE = pgroonga.similar_text_array,
			LEFTARG = text[],
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.similar_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_similar_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		-- Deprecated since 1.2.2.
		CREATE OPERATOR public.&~? (
			PROCEDURE = pgroonga.similar_varchar,
			LEFTARG = varchar,
			RIGHTARG = varchar,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR public.&@* (
			PROCEDURE = pgroonga.similar_varchar,
			LEFTARG = varchar,
			RIGHTARG = varchar,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.prefix_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&^ (
			PROCEDURE = pgroonga.prefix_text,
			LEFTARG = text,
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.prefix_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&^ (
			PROCEDURE = pgroonga.prefix_text_array,
			LEFTARG = text[],
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		/* Deprecated since 1.2.1. */
		CREATE OPERATOR public.&^> (
			PROCEDURE = pgroonga.prefix_text_array,
			LEFTARG = text[],
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.prefix_rk_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&^~ (
			PROCEDURE = pgroonga.prefix_rk_text,
			LEFTARG = text,
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.prefix_rk_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&^~ (
			PROCEDURE = pgroonga.prefix_rk_text_array,
			LEFTARG = text[],
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		/* Deprecated since 1.2.1. */
		CREATE OPERATOR public.&^~> (
			PROCEDURE = pgroonga.prefix_rk_text_array,
			LEFTARG = text[],
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.script_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_script_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&` (
			PROCEDURE = pgroonga.script_text,
			LEFTARG = text,
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.script_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_script_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&` (
			PROCEDURE = pgroonga.script_text_array,
			LEFTARG = text[],
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.script_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_script_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&` (
			PROCEDURE = pgroonga.script_varchar,
			LEFTARG = varchar,
			RIGHTARG = varchar,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND THEN
		CREATE FUNCTION pgroonga.script_jsonb(jsonb, text)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_script_jsonb'
			LANGUAGE C
			IMMUTABLE
			STRICT;

		IF current_schema() <> 'public' THEN
			CREATE OPERATOR public.&` (
				PROCEDURE = pgroonga.script_jsonb,
				LEFTARG = jsonb,
				RIGHTARG = text,
				RESTRICT = contsel,
				JOIN = contjoinsel
			);
		END IF;
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.match_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		/* Deprecated since 1.2.1. */
		CREATE OPERATOR public.&@> (
			PROCEDURE = pgroonga.match_in_text,
			LEFTARG = text,
			RIGHTARG = text[],
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR public.&@| (
			PROCEDURE = pgroonga.match_in_text,
			LEFTARG = text,
			RIGHTARG = text[],
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.match_in_text_array(text[], text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_in_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&@| (
			PROCEDURE = pgroonga.match_in_text_array,
			LEFTARG = text[],
			RIGHTARG = text[],
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.match_in_varchar(varchar, varchar[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_in_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&@| (
			PROCEDURE = pgroonga.match_in_varchar,
			LEFTARG = varchar,
			RIGHTARG = varchar[],
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.query_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		/* Deprecated since 1.2.1. */
		CREATE OPERATOR public.&?> (
			PROCEDURE = pgroonga.query_in_text,
			LEFTARG = text,
			RIGHTARG = text[],
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		-- Deprecated since 1.2.2.
		CREATE OPERATOR public.&?| (
			PROCEDURE = pgroonga.query_in_text,
			LEFTARG = text,
			RIGHTARG = text[],
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR public.&@~| (
			PROCEDURE = pgroonga.query_in_text,
			LEFTARG = text,
			RIGHTARG = text[],
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.query_in_text_array(text[], text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_in_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		-- Deprecated since 1.2.2.
		CREATE OPERATOR public.&?| (
			PROCEDURE = pgroonga.query_in_text_array,
			LEFTARG = text[],
			RIGHTARG = text[],
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR public.&@~| (
			PROCEDURE = pgroonga.query_in_text_array,
			LEFTARG = text[],
			RIGHTARG = text[],
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.query_in_varchar(varchar, varchar[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_in_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		-- Deprecated since 1.2.2.
		CREATE OPERATOR public.&?| (
			PROCEDURE = pgroonga.query_in_varchar,
			LEFTARG = varchar,
			RIGHTARG = varchar[],
			RESTRICT = contsel,
			JOIN = contjoinsel
		);

		CREATE OPERATOR public.&@~| (
			PROCEDURE = pgroonga.query_in_varchar,
			LEFTARG = varchar,
			RIGHTARG = varchar[],
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.prefix_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&^| (
			PROCEDURE = pgroonga.prefix_in_text,
			LEFTARG = text,
			RIGHTARG = text[],
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.prefix_in_text_array(text[], text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_in_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&^| (
			PROCEDURE = pgroonga.prefix_in_text_array,
			LEFTARG = text[],
			RIGHTARG = text[],
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.prefix_rk_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&^~| (
			PROCEDURE = pgroonga.prefix_rk_in_text,
			LEFTARG = text,
			RIGHTARG = text[],
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.prefix_rk_in_text_array(text[], text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_rk_in_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&^~| (
			PROCEDURE = pgroonga.prefix_rk_in_text_array,
			LEFTARG = text[],
			RIGHTARG = text[],
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.regexp_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_regexp_text'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&~ (
			PROCEDURE = pgroonga.regexp_text,
			LEFTARG = text,
			RIGHTARG = text,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;

CREATE FUNCTION pgroonga.regexp_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_regexp_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT;

DO LANGUAGE plpgsql $$
BEGIN
	IF current_schema() <> 'public' THEN
		CREATE OPERATOR public.&~ (
			PROCEDURE = pgroonga.regexp_varchar,
			LEFTARG = varchar,
			RIGHTARG = varchar,
			RESTRICT = contsel,
			JOIN = contjoinsel
		);
	END IF;
END;
$$;


/* v1 */
CREATE OPERATOR CLASS pgroonga.text_full_text_search_ops FOR TYPE text
	USING pgroonga AS
		OPERATOR 6 ~~,
		OPERATOR 7 ~~*,
		OPERATOR 8 %%,
		OPERATOR 9 @@,
		OPERATOR 12 &@,
		OPERATOR 13 &?, -- For backward compatibility
		OPERATOR 28 &@~;

CREATE OPERATOR CLASS pgroonga.text_array_full_text_search_ops
	FOR TYPE text[]
	USING pgroonga AS
		OPERATOR 8 %% (text[], text),
		OPERATOR 9 @@ (text[], text),
		OPERATOR 12 &@ (text[], text),
		OPERATOR 13 &? (text[], text), -- For backward compatibility
		OPERATOR 28 &@~ (text[], text);

CREATE OPERATOR CLASS pgroonga.varchar_full_text_search_ops FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 8 %%,
		OPERATOR 9 @@,
		OPERATOR 12 &@,
		OPERATOR 13 &?, -- For backward compatibility
		OPERATOR 28 &@~;

CREATE OPERATOR CLASS pgroonga.varchar_ops FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 1 < (text, text),
		OPERATOR 2 <= (text, text),
		OPERATOR 3 = (text, text),
		OPERATOR 4 >= (text, text),
		OPERATOR 5 > (text, text);

CREATE OPERATOR CLASS pgroonga.varchar_array_ops
	FOR TYPE varchar[]
	USING pgroonga AS
		OPERATOR 8 %% (varchar[], varchar),
		OPERATOR 23 &> (varchar[], varchar);

CREATE OPERATOR CLASS pgroonga.bool_ops FOR TYPE bool
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.int2_ops FOR TYPE int2
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.int4_ops FOR TYPE int4
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.int8_ops FOR TYPE int8
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.float4_ops FOR TYPE float4
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.float8_ops FOR TYPE float8
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.timestamp_ops FOR TYPE timestamp
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE OPERATOR CLASS pgroonga.timestamptz_ops FOR TYPE timestamptz
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND THEN
		CREATE FUNCTION pgroonga.match_script_jsonb(jsonb, text)
			RETURNS bool
			AS 'MODULE_PATHNAME', 'pgroonga_match_script_jsonb'
			LANGUAGE C
			IMMUTABLE
			STRICT;

		IF current_schema() <> 'public' THEN
			CREATE OPERATOR public.@@ (
				PROCEDURE = pgroonga.match_script_jsonb,
				LEFTARG = jsonb,
				RIGHTARG = text,
				RESTRICT = contsel,
				JOIN = contjoinsel
			);
		END IF;

		CREATE OPERATOR CLASS pgroonga.jsonb_ops FOR TYPE jsonb
			USING pgroonga AS
				OPERATOR 9 @@ (jsonb, text),
				OPERATOR 11 @>,
				OPERATOR 12 &@ (jsonb, text),
				OPERATOR 13 &? (jsonb, text), -- For backward compatibility
				OPERATOR 15 &` (jsonb, text),
				OPERATOR 28 &@~ (jsonb, text);
	END IF;
END;
$$;

CREATE OPERATOR CLASS pgroonga.text_regexp_ops FOR TYPE text
	USING pgroonga AS
		OPERATOR 6 ~~,
		OPERATOR 7 ~~*,
		OPERATOR 10 @~,
		OPERATOR 22 &~;

CREATE OPERATOR CLASS pgroonga.varchar_regexp_ops FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 10 @~,
		OPERATOR 22 &~;

/* v2 */
CREATE OPERATOR CLASS pgroonga.text_full_text_search_ops_v2 FOR TYPE text
	USING pgroonga AS
		OPERATOR 6 ~~,
		OPERATOR 7 ~~*,
		OPERATOR 8 %%, -- For backward compatibility
		OPERATOR 9 @@, -- For backward compatibility
		OPERATOR 12 &@,
		OPERATOR 13 &?, -- For backward compatibility
		OPERATOR 14 &~?, -- For backward compatibility
		OPERATOR 15 &`,
		OPERATOR 18 &@| (text, text[]),
		OPERATOR 19 &?| (text, text[]), -- For backward compatibility
		OPERATOR 26 &@> (text, text[]), -- For backward compatibility
		OPERATOR 27 &?> (text, text[]), -- For backward compatibility
		OPERATOR 28 &@~,
		OPERATOR 29 &@*,
		OPERATOR 30 &@~| (text, text[]);

CREATE OPERATOR CLASS pgroonga.text_array_full_text_search_ops_v2
	FOR TYPE text[]
	USING pgroonga AS
		OPERATOR 8 %% (text[], text), -- For backward compatibility
		OPERATOR 9 @@ (text[], text), -- For backward compatibility
		OPERATOR 12 &@ (text[], text),
		OPERATOR 13 &? (text[], text), -- For backward compatibility
		OPERATOR 14 &~? (text[], text), -- For backward compatibility
		OPERATOR 15 &` (text[], text),
		OPERATOR 18 &@| (text[], text[]),
		OPERATOR 19 &?| (text[], text[]), -- For backward compatibility
		OPERATOR 28 &@~ (text[], text),
		OPERATOR 29 &@* (text[], text),
		OPERATOR 30 &@~| (text[], text[]);

CREATE OPERATOR CLASS pgroonga.text_term_search_ops_v2 FOR TYPE text
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >,
		OPERATOR 16 &^,
		OPERATOR 17 &^~,
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

CREATE OPERATOR CLASS pgroonga.text_regexp_ops_v2 FOR TYPE text
	USING pgroonga AS
		OPERATOR 6 ~~,
		OPERATOR 7 ~~*,
		OPERATOR 10 @~, -- For backward compatibility
		OPERATOR 22 &~;

CREATE OPERATOR CLASS pgroonga.varchar_full_text_search_ops_v2
	FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 8 %%, -- For backward compatibility
		OPERATOR 9 @@, -- For backward compatibility
		OPERATOR 12 &@,
		OPERATOR 13 &?, -- For backward compatibility
		OPERATOR 14 &~?, -- For backward compatibility
		OPERATOR 15 &`,
		OPERATOR 18 &@| (varchar, varchar[]),
		OPERATOR 19 &?| (varchar, varchar[]), -- For backward compatibility
		OPERATOR 28 &@~,
		OPERATOR 29 &@*,
		OPERATOR 30 &@~| (varchar, varchar[]);

CREATE OPERATOR CLASS pgroonga.varchar_array_term_search_ops_v2
	FOR TYPE varchar[]
	USING pgroonga AS
		OPERATOR 8 %% (varchar[], varchar), -- For backward compatibility
		OPERATOR 23 &> (varchar[], varchar);

CREATE OPERATOR CLASS pgroonga.varchar_regexp_ops_v2 FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 10 @~, -- For backward compatibility
		OPERATOR 22 &~;

DO LANGUAGE plpgsql $$
BEGIN
	PERFORM 1
		FROM pg_type
		WHERE typname = 'jsonb';

	IF FOUND THEN
		CREATE OPERATOR CLASS pgroonga.jsonb_ops_v2
			FOR TYPE jsonb
			USING pgroonga AS
				OPERATOR 9 @@ (jsonb, text), -- For backward compatibility
				OPERATOR 11 @>,
				OPERATOR 12 &@ (jsonb, text),
				OPERATOR 13 &? (jsonb, text), -- For backward compatibility
				OPERATOR 15 &` (jsonb, text),
				OPERATOR 28 &@~ (jsonb, text);
	END IF;
END;
$$;
