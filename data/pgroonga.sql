-- Deprecated since 3.1.6. Use pgroonga_condition instead.
CREATE TYPE pgroonga_full_text_search_condition AS (
	query text,
	weigths int[],
	indexName text
);

-- Deprecated since 3.1.6. Use pgroonga_condition instead.
CREATE TYPE pgroonga_full_text_search_condition_with_scorers AS (
	query text,
	weigths int[],
	scorers text[],
	indexName text
);

CREATE TYPE pgroonga_condition AS (
	query text,
	weigths int[],
	scorers text[],
	schema_name text,
	index_name text,
	column_name text,
	fuzzy_max_distance_ratio float4
);

CREATE FUNCTION pgroonga_condition(query text = null,
				   weights int[] = null,
				   scorers text[] = null,
				   schema_name text = null,
				   index_name text = null,
				   column_name text = null,
				   fuzzy_max_distance_ratio float4 = null)
	RETURNS pgroonga_condition
	LANGUAGE SQL
	AS $$
		SELECT (
			query,
			weights,
			scorers,
			schema_name,
			index_name,
			column_name,
			fuzzy_max_distance_ratio
		)::pgroonga_condition
	$$
	IMMUTABLE
	LEAKPROOF
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_score("row" record)
	RETURNS float8
	AS 'MODULE_PATHNAME', 'pgroonga_score_row'
	LANGUAGE C
	VOLATILE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_score(tableoid oid, ctid tid)
	RETURNS float8
	AS 'MODULE_PATHNAME', 'pgroonga_score_ctid'
	LANGUAGE C
	VOLATILE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_table_name(indexName cstring)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_table_name'
	LANGUAGE C
	STABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_command(groongaCommand text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_command'
	LANGUAGE C
	VOLATILE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_command(groongaCommand text, arguments text[])
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_command'
	LANGUAGE C
	VOLATILE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_query_expand(tableName cstring,
				      termColumnName text,
				      synonymsColumnName text,
				      query text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_query_expand'
	LANGUAGE C
	STABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_snippet_html(target text, keywords text[], width integer DEFAULT 200)
	RETURNS text[]
	AS 'MODULE_PATHNAME', 'pgroonga_snippet_html'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_highlight_html(target text, keywords text[])
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_highlight_html_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_highlight_html(target text,
				        keywords text[],
				        indexName cstring)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_highlight_html_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_highlight_html(targets text[], keywords text[])
	RETURNS text[]
	AS 'MODULE_PATHNAME', 'pgroonga_highlight_html_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_highlight_html(targets text[],
				        keywords text[],
				        indexName cstring)
	RETURNS text[]
	AS 'MODULE_PATHNAME', 'pgroonga_highlight_html_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_match_positions_byte(target text, keywords text[])
	RETURNS integer[2][]
	AS 'MODULE_PATHNAME', 'pgroonga_match_positions_byte'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_match_positions_byte(target text,
					      keywords text[],
					      indexName cstring)
	RETURNS integer[2][]
	AS 'MODULE_PATHNAME', 'pgroonga_match_positions_byte'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_match_positions_character(target text, keywords text[])
	RETURNS integer[2][]
	AS 'MODULE_PATHNAME', 'pgroonga_match_positions_character'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_match_positions_character(target text,
						   keywords text[],
						   indexName cstring)
	RETURNS integer[2][]
	AS 'MODULE_PATHNAME', 'pgroonga_match_positions_character'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_query_extract_keywords(query text,
						index_name text DEFAULT '')
	RETURNS text[]
	AS 'MODULE_PATHNAME', 'pgroonga_query_extract_keywords'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_flush(indexName cstring)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_flush'
	LANGUAGE C
	VOLATILE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_command_escape_value(value text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_command_escape_value'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_query_escape(query text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_query_escape'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_escape(value text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_string'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_escape(value text, special_characters text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_string'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_escape(value boolean)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_boolean'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_escape(value int2)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_int2'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_escape(value int4)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_int4'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_escape(value int8)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_int8'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_escape(value float4)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_float4'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_escape(value float8)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_float8'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_escape(value timestamp)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_timestamptz'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_escape(value timestamptz)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_escape_timestamptz'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

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
	STRICT
	PARALLEL SAFE;

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
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_normalize(target text, normalizerName text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_normalize'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_tokenize(target text, VARIADIC options text[])
	RETURNS json[]
	AS 'MODULE_PATHNAME', 'pgroonga_tokenize'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_vacuum()
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_vacuum'
	LANGUAGE C
	VOLATILE
	STRICT;

CREATE FUNCTION pgroonga_index_column_name(indexName cstring, columnName text)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_index_column_name_name'
	LANGUAGE C
	STABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_index_column_name(indexName cstring, columnIndex int4)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_index_column_name_index'
	LANGUAGE C
	STABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_result_to_recordset(result jsonb)
	RETURNS SETOF RECORD
	AS 'MODULE_PATHNAME', 'pgroonga_result_to_recordset'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_result_to_jsonb_objects(result jsonb)
	RETURNS jsonb
	AS 'MODULE_PATHNAME', 'pgroonga_result_to_jsonb_objects'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_wal_status()
	RETURNS TABLE(
			name text,
			oid oid,
			current_block int8,
			current_offset int8,
			current_size int8,
			last_block int8,
			last_offset int8,
			last_size int8
		)
	AS 'MODULE_PATHNAME', 'pgroonga_wal_status'
	LANGUAGE C
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_wal_set_applied_position(indexName cstring, "block" bigint, "offset" bigint)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_wal_set_applied_position_index'
	LANGUAGE C
	STRICT;

CREATE FUNCTION pgroonga_wal_set_applied_position(indexName cstring)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_wal_set_applied_position_index_last'
	LANGUAGE C
	STRICT;

CREATE FUNCTION pgroonga_wal_set_applied_position("block" bigint, "offset" bigint)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_wal_set_applied_position_all'
	LANGUAGE C
	STRICT;

CREATE FUNCTION pgroonga_wal_set_applied_position()
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_wal_set_applied_position_all_last'
	LANGUAGE C
	STRICT;

CREATE FUNCTION pgroonga_list_lagged_indexes()
	RETURNS SETOF text AS '
		SELECT name FROM pgroonga_wal_status()
		WHERE current_block != last_block
			OR current_offset != last_offset
			OR current_size != last_size
			OR EXISTS(
				SELECT 1 FROM pg_stat_wal_receiver
				WHERE flushed_lsn != latest_end_lsn
			);
	' LANGUAGE SQL
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_list_broken_indexes()
	RETURNS SETOF text
	AS 'MODULE_PATHNAME', 'pgroonga_list_broken_indexes'
	LANGUAGE C
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_language_model_vectorize(model_name text, target text)
	RETURNS float4[]
	AS 'MODULE_PATHNAME', 'pgroonga_language_model_vectorize'
	LANGUAGE C
	STRICT
	PARALLEL SAFE;

/* v1 */
CREATE FUNCTION pgroonga_match_term(target text, term text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_term_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_match_term(target text[], term text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_term_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_match_term(target varchar, term varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_term_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_match_term(target varchar[], term varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_term_varchar_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

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
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_match_query(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_query_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_match_query(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_query_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

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
	STRICT
	PARALLEL SAFE;

CREATE FUNCTION pgroonga_match_regexp(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_regexp_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_text_condition_with_scorers,
	LEFTARG = text,
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_text_condition
	(target text, condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_text_condition,
	LEFTARG = text,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_text_array_condition_with_scorers,
	LEFTARG = text[],
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_text_array_condition
	(target text[], condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_text_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_text_array_condition,
	LEFTARG = text[],
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_varchar_condition_with_scorers,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_varchar_condition
	(target varchar, condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_varchar_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_varchar_condition,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_contain_varchar_array(varchar[], varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_contain_varchar_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &> (
	PROCEDURE = pgroonga_contain_varchar_array,
	LEFTARG = varchar[],
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_jsonb(jsonb, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_jsonb'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@ (
	PROCEDURE = pgroonga_match_jsonb,
	LEFTARG = jsonb,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_text_condition_with_scorers,
	LEFTARG = text,
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_text_condition
	(target text, condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_text_condition,
	LEFTARG = text,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_text_array_condition_with_scorers,
	LEFTARG = text[],
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_text_array_condition
	(targets text[], condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_text_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_text_array_condition,
	LEFTARG = text[],
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_varchar_condition_with_scorers,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_full_text_search_condition_with_scorers,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_varchar_condition
	(target varchar, condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_varchar_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &@~ (
	PROCEDURE = pgroonga_query_varchar_condition,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_query_jsonb(jsonb, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_query_jsonb'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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

CREATE FUNCTION pgroonga_similar_text(text, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_similar_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 10000;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 10000;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 10000;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_text,
	LEFTARG = text,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_text_condition
	(text, condition pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_text_condition,
	LEFTARG = text,
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_text_condition
	(text, condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_text_condition,
	LEFTARG = text,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_text_array(text[], text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_text_array,
	LEFTARG = text[],
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_text_array_condition(text[], pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_text_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_text_array_condition,
	LEFTARG = text[],
	RIGHTARG = pgroonga_condition,
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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_varchar_condition
	(target varchar, conditoin pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_varchar_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_varchar_condition,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_varchar_condition
	(target varchar, conditoin pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_varchar_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_varchar_condition,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_varchar_array(varchar[], varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_varchar_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_varchar_array,
	LEFTARG = varchar[],
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_prefix_varchar_array_condition(varchar[], pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_prefix_varchar_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &^ (
	PROCEDURE = pgroonga_prefix_varchar_array_condition,
	LEFTARG = varchar[],
	RIGHTARG = pgroonga_condition,
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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &` (
	PROCEDURE = pgroonga_script_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_script_jsonb(jsonb, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_script_jsonb'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &` (
	PROCEDURE = pgroonga_script_jsonb,
	LEFTARG = jsonb,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_match_in_text(text, text[])
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_in_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &~ (
	PROCEDURE = pgroonga_regexp_text,
	LEFTARG = text,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_regexp_text_array(targets text[], pattern text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_regexp_text_array'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &~ (
	PROCEDURE = pgroonga_regexp_text_array,
	LEFTARG = text[],
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_regexp_text_array_condition
        (targets text[], pattern pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_regexp_text_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &~ (
	PROCEDURE = pgroonga_regexp_text_array_condition,
	LEFTARG = text[],
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_regexp_varchar(varchar, varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_regexp_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

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
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &~| (
	PROCEDURE = pgroonga_regexp_in_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar[],
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_equal_text(target text, other text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_text'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &= (
	PROCEDURE = pgroonga_equal_text,
	LEFTARG = text,
	RIGHTARG = text,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_equal_text_condition
	(target text, condition pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &= (
	PROCEDURE = pgroonga_equal_text_condition,
	LEFTARG = text,
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_equal_text_condition
	(target text, condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &= (
	PROCEDURE = pgroonga_equal_text_condition,
	LEFTARG = text,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_equal_varchar(target varchar, other varchar)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_varchar'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &= (
	PROCEDURE = pgroonga_equal_varchar,
	LEFTARG = varchar,
	RIGHTARG = varchar,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_equal_varchar_condition
	(target varchar, condition pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &= (
	PROCEDURE = pgroonga_equal_varchar_condition,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_equal_varchar_condition
	(target varchar, condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_text_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &= (
	PROCEDURE = pgroonga_equal_varchar_condition,
	LEFTARG = varchar,
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

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

CREATE FUNCTION pgroonga_equal_query_text_array_condition
	(targets text[], condition pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_query_text_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &=~ (
	PROCEDURE = pgroonga_equal_query_text_array_condition,
	LEFTARG = text[],
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_equal_query_text_array_condition
	(targets text[], condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_query_text_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &=~ (
	PROCEDURE = pgroonga_equal_query_text_array_condition,
	LEFTARG = text[],
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

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

CREATE FUNCTION pgroonga_equal_query_varchar_array_condition
	(targets varchar[], condition pgroonga_full_text_search_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_query_varchar_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &=~ (
	PROCEDURE = pgroonga_equal_query_varchar_array_condition,
	LEFTARG = varchar[],
	RIGHTARG = pgroonga_full_text_search_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);

CREATE FUNCTION pgroonga_equal_query_varchar_array_condition
	(targets varchar[], condition pgroonga_condition)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_equal_query_varchar_array_condition'
	LANGUAGE C
	IMMUTABLE
	STRICT
	LEAKPROOF
	PARALLEL SAFE
	COST 300;

CREATE OPERATOR &=~ (
	PROCEDURE = pgroonga_equal_query_varchar_array_condition,
	LEFTARG = varchar[],
	RIGHTARG = pgroonga_condition,
	RESTRICT = contsel,
	JOIN = contjoinsel
);


DROP ACCESS METHOD IF EXISTS pgroonga CASCADE;
CREATE FUNCTION pgroonga_handler(internal)
	RETURNS index_am_handler
	AS 'MODULE_PATHNAME', 'pgroonga_handler'
	LANGUAGE C;
CREATE ACCESS METHOD pgroonga
	TYPE INDEX
	HANDLER pgroonga_handler;

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

CREATE OPERATOR CLASS pgroonga_uuid_ops DEFAULT FOR TYPE uuid
	USING pgroonga AS
		OPERATOR 1 <,
		OPERATOR 2 <=,
		OPERATOR 3 =,
		OPERATOR 4 >=,
		OPERATOR 5 >;

CREATE FUNCTION pgroonga_match_script_jsonb(jsonb, text)
	RETURNS bool
	AS 'MODULE_PATHNAME', 'pgroonga_match_script_jsonb'
	LANGUAGE C
	IMMUTABLE
	STRICT
	PARALLEL SAFE;

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
		-- For backward compatibility
		OPERATOR 31 &@ (text, pgroonga_full_text_search_condition),
		-- For backward compatibility
		OPERATOR 32 &@~ (text, pgroonga_full_text_search_condition),
		-- For backward compatibility
		OPERATOR 33 &@ (text, pgroonga_full_text_search_condition_with_scorers),
		-- For backward compatibility
		OPERATOR 34 &@~ (text, pgroonga_full_text_search_condition_with_scorers),
		OPERATOR 42 &@ (text, pgroonga_condition),
		OPERATOR 43 &@~ (text, pgroonga_condition);


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
		-- For backward compatibility
		OPERATOR 31 &@ (text[], pgroonga_full_text_search_condition),
		-- For backward compatibility
		OPERATOR 32 &@~ (text[], pgroonga_full_text_search_condition),
		-- For backward compatibility
		OPERATOR 33 &@ (text[], pgroonga_full_text_search_condition_with_scorers),
		-- For backward compatibility
		OPERATOR 34 &@~ (text[], pgroonga_full_text_search_condition_with_scorers),
		OPERATOR 42 &@ (text[], pgroonga_condition),
		OPERATOR 43 &@~ (text[], pgroonga_condition);

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
		OPERATOR 36 !&^| (text, text[]),
		-- For backward compatibility
		OPERATOR 37 &^ (text, pgroonga_full_text_search_condition),
		OPERATOR 38 &=,
		-- For backward compatibility
		OPERATOR 39 &= (text, pgroonga_full_text_search_condition),
		OPERATOR 44 &^ (text, pgroonga_condition),
		OPERATOR 45 &= (text, pgroonga_condition);

CREATE OPERATOR CLASS pgroonga_text_array_term_search_ops_v2 FOR TYPE text[]
	USING pgroonga AS
		OPERATOR 11 @> (anyarray, anyarray),
		OPERATOR 16 &^ (text[], text),
		OPERATOR 17 &^~ (text[], text),
		OPERATOR 20 &^| (text[], text[]),
		OPERATOR 21 &^~| (text[], text[]),
		OPERATOR 24 &^> (text[], text), -- For backward compatibility
		OPERATOR 25 &^~> (text[], text), -- For backward compatibility
		OPERATOR 40 &=~ (text[], text),
		-- For backward compatibility
		OPERATOR 41 &=~ (text[], pgroonga_full_text_search_condition),
		OPERATOR 44 &^ (text[], pgroonga_condition),
		OPERATOR 46 &=~ (text[], pgroonga_condition);

CREATE OPERATOR CLASS pgroonga_text_regexp_ops_v2 FOR TYPE text
	USING pgroonga AS
		OPERATOR 6 ~~,
		OPERATOR 7 ~~*,
		OPERATOR 10 @~, -- For backward compatibility
		OPERATOR 22 &~,
		OPERATOR 35 &~| (text, text[]);

CREATE OPERATOR CLASS pgroonga_text_array_regexp_ops_v2 FOR TYPE text[]
	USING pgroonga AS
		OPERATOR 22 &~ (text[], text),
		OPERATOR 47 &~ (text[], pgroonga_condition);

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
		OPERATOR 21 &^~| (varchar, varchar[]),
		-- For backward compatibility
		OPERATOR 37 &^ (varchar, pgroonga_full_text_search_condition),
		OPERATOR 38 &=,
		-- For backward compatibility
		OPERATOR 39 &= (varchar, pgroonga_full_text_search_condition),
		OPERATOR 44 &^ (varchar, pgroonga_condition),
		OPERATOR 45 &= (varchar, pgroonga_condition);

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
		-- For backward compatibility
		OPERATOR 31 &@ (varchar, pgroonga_full_text_search_condition),
		-- For backward compatibility
		OPERATOR 32 &@~ (varchar, pgroonga_full_text_search_condition),
		-- For backward compatibility
		OPERATOR 33 &@ (varchar, pgroonga_full_text_search_condition_with_scorers),
		-- For backward compatibility
		OPERATOR 34 &@~ (varchar, pgroonga_full_text_search_condition_with_scorers),
		OPERATOR 42 &@ (varchar, pgroonga_condition),
		OPERATOR 43 &@~ (varchar, pgroonga_condition);

CREATE OPERATOR CLASS pgroonga_varchar_array_term_search_ops_v2
	DEFAULT FOR TYPE varchar[]
	USING pgroonga AS
		OPERATOR 8 %% (varchar[], varchar), -- For backward compatibility
		OPERATOR 11 @> (anyarray, anyarray),
		OPERATOR 16 &^ (varchar[], varchar),
		OPERATOR 17 &^~ (varchar[], varchar),
		OPERATOR 20 &^| (varchar[], varchar[]),
		OPERATOR 21 &^~| (varchar[], varchar[]),
		OPERATOR 23 &> (varchar[], varchar),
		OPERATOR 24 &^> (varchar[], varchar), -- For backward compatibility
		OPERATOR 25 &^~> (varchar[], varchar), -- For backward compatibility
		OPERATOR 40 &=~ (varchar[], text),
		-- For backward compatibility
		OPERATOR 41 &=~ (varchar[], pgroonga_full_text_search_condition),
		OPERATOR 44 &^ (varchar[], pgroonga_condition),
		OPERATOR 46 &=~ (varchar[], pgroonga_condition);

CREATE OPERATOR CLASS pgroonga_varchar_regexp_ops_v2 FOR TYPE varchar
	USING pgroonga AS
		OPERATOR 10 @~, -- For backward compatibility
		OPERATOR 22 &~,
		OPERATOR 35 &~| (varchar, varchar[]);

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
