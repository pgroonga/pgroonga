-- Upgrade SQL

ALTER FUNCTION pgroonga_score("row" record) PARALLEL SAFE;
ALTER FUNCTION pgroonga_score(tableoid oid, ctid tid) PARALLEL SAFE;
ALTER FUNCTION pgroonga_table_name(indexName cstring) PARALLEL SAFE;
ALTER FUNCTION pgroonga_command(groongaCommand text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_command(groongaCommand text, arguments text[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_query_expand(tableName cstring,
				     termColumnName text,
				     synonymsColumnName text,
				     query text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_snippet_html(target text, keywords text[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_highlight_html(target text, keywords text[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_highlight_html(target text,
				       keywords text[],
				       indexName cstring) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_positions_byte(target text, keywords text[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_positions_byte(target text,
					     keywords text[],
					     indexName cstring) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_positions_character(target text, keywords text[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_positions_character(target text,
						  keywords text[],
						  indexName cstring) PARALLEL SAFE;
ALTER FUNCTION pgroonga_query_extract_keywords(query text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_flush(indexName cstring) PARALLEL SAFE;
ALTER FUNCTION pgroonga_command_escape_value(value text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_query_escape(query text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_escape(value text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_escape(value text, special_characters text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_escape(value boolean) PARALLEL SAFE;
ALTER FUNCTION pgroonga_escape(value int2) PARALLEL SAFE;
ALTER FUNCTION pgroonga_escape(value int4) PARALLEL SAFE;
ALTER FUNCTION pgroonga_escape(value int8) PARALLEL SAFE;
ALTER FUNCTION pgroonga_escape(value float4) PARALLEL SAFE;
ALTER FUNCTION pgroonga_escape(value float8) PARALLEL SAFE;
ALTER FUNCTION pgroonga_escape(value timestamp) PARALLEL SAFE;
ALTER FUNCTION pgroonga_escape(value timestamptz) PARALLEL SAFE;
ALTER FUNCTION pgroonga_is_writable() PARALLEL SAFE;
ALTER FUNCTION pgroonga_normalize(target text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_normalize(target text, normalizerName text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_tokenize(target text, VARIADIC options text[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_index_column_name(indexName cstring, columnName text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_index_column_name(indexName cstring, columnIndex int4) PARALLEL SAFE;
ALTER FUNCTION pgroonga_result_to_recordset(result jsonb) PARALLEL SAFE;
ALTER FUNCTION pgroonga_result_to_jsonb_objects(result jsonb) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_term(target text, term text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_term(target text[], term text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_term(target varchar, term varchar) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_term(target varchar[], term varchar) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_query(text, text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_query(text[], text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_query(varchar, varchar) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_regexp(text, text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_regexp(varchar, varchar) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_text(text, text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_text_condition
	(target text, condition pgroonga_full_text_search_condition) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_text_condition_with_scorers
	(target text,
	 condition pgroonga_full_text_search_condition_with_scorers) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_text_array(text[], text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_text_array_condition
	(target text[], condition pgroonga_full_text_search_condition) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_text_array_condition_with_scorers
	(target text[],
	 condition pgroonga_full_text_search_condition_with_scorers) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_varchar(varchar, varchar) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_varchar_condition
	(target varchar, condition pgroonga_full_text_search_condition) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_varchar_condition_with_scorers
	(target varchar,
	 condition pgroonga_full_text_search_condition_with_scorers) PARALLEL SAFE;
ALTER FUNCTION pgroonga_contain_varchar_array(varchar[], varchar) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_jsonb(jsonb, text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_query_text(text, text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_query_text_condition
	(target text, condition pgroonga_full_text_search_condition) PARALLEL SAFE;
ALTER FUNCTION pgroonga_query_text_condition_with_scorers
	(target text,
	 condition pgroonga_full_text_search_condition_with_scorers) PARALLEL SAFE;
ALTER FUNCTION pgroonga_query_text_array(text[], text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_query_text_array_condition
	(targets text[], condition pgroonga_full_text_search_condition) PARALLEL SAFE;
ALTER FUNCTION pgroonga_query_text_array_condition_with_scorers
	(targets text[],
	 condition pgroonga_full_text_search_condition_with_scorers) PARALLEL SAFE;
ALTER FUNCTION pgroonga_query_varchar(varchar, varchar) PARALLEL SAFE;
ALTER FUNCTION pgroonga_query_varchar_condition
	(target varchar, condition pgroonga_full_text_search_condition) PARALLEL SAFE;
ALTER FUNCTION pgroonga_query_varchar_condition_with_scorers
	(target varchar,
	 condition pgroonga_full_text_search_condition_with_scorers) PARALLEL SAFE;
ALTER FUNCTION pgroonga_query_jsonb(jsonb, text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_similar_text(text, text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_similar_text_array(text[], text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_similar_varchar(varchar, varchar) PARALLEL SAFE;
ALTER FUNCTION pgroonga_prefix_text(text, text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_prefix_text_array(text[], text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_prefix_varchar(varchar, varchar) PARALLEL SAFE;
ALTER FUNCTION pgroonga_prefix_varchar_array(varchar[], varchar) PARALLEL SAFE;
ALTER FUNCTION pgroonga_prefix_rk_text(text, text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_prefix_rk_text_array(text[], text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_prefix_rk_varchar(varchar, varchar) PARALLEL SAFE;
ALTER FUNCTION pgroonga_prefix_rk_varchar_array(varchar[], varchar) PARALLEL SAFE;
ALTER FUNCTION pgroonga_script_text(text, text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_script_text_array(text[], text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_script_varchar(varchar, varchar) PARALLEL SAFE;
ALTER FUNCTION pgroonga_script_jsonb(jsonb, text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_in_text(text, text[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_in_text_array(text[], text[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_in_varchar(varchar, varchar[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_query_in_text(text, text[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_query_in_text_array(text[], text[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_query_in_varchar(varchar, varchar[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_prefix_in_text(text, text[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_not_prefix_in_text(text, text[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_prefix_in_text_array(text[], text[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_prefix_in_varchar(varchar, varchar[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_prefix_in_varchar_array(varchar[], varchar[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_prefix_rk_in_text(text, text[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_prefix_rk_in_text_array(text[], text[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_prefix_rk_in_varchar(varchar, varchar[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_prefix_rk_in_varchar_array(varchar[], varchar[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_regexp_text(text, text) PARALLEL SAFE;
ALTER FUNCTION pgroonga_regexp_varchar(varchar, varchar) PARALLEL SAFE;
ALTER FUNCTION pgroonga_regexp_in_text(text, text[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_regexp_in_varchar(varchar, varchar[]) PARALLEL SAFE;
ALTER FUNCTION pgroonga_handler(internal) PARALLEL SAFE;
ALTER FUNCTION pgroonga_match_script_jsonb(jsonb, text) PARALLEL SAFE;
