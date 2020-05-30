-- Upgrade SQL

CREATE FUNCTION pgroonga_highlight_html(target text,
				        keywords text[],
				        indexName cstring)
	RETURNS text
	AS 'MODULE_PATHNAME', 'pgroonga_highlight_html'
	LANGUAGE C
	IMMUTABLE
	STRICT;

ALTER FUNCTION pgroonga_match_term(target text, term text)
	COST 100;
ALTER FUNCTION pgroonga_match_term(target text[], term text)
	COST 100;
ALTER FUNCTION pgroonga_match_term(target varchar, term varchar)
	COST 100;
ALTER FUNCTION pgroonga_match_term(target varchar[], term varchar)
	COST 100;
ALTER FUNCTION pgroonga_match_query(text, text)
	COST 100;
ALTER FUNCTION pgroonga_match_query(text[], text)
	COST 100;
ALTER FUNCTION pgroonga_match_query(varchar, varchar)
	COST 100;
ALTER FUNCTION pgroonga_match_regexp(text, text)
	COST 100;
ALTER FUNCTION pgroonga_match_regexp(varchar, varchar)
	COST 100;
ALTER FUNCTION pgroonga_match_text(text, text)
	COST 100;
ALTER FUNCTION pgroonga_match_text_condition
	(target text, condition pgroonga_full_text_search_condition)
	COST 100;
ALTER FUNCTION pgroonga_match_text_condition_with_scorers
	(target text,
	 condition pgroonga_full_text_search_condition_with_scorers)
	COST 100;
ALTER FUNCTION pgroonga_match_text_array(text[], text)
	COST 100;
ALTER FUNCTION pgroonga_match_text_array_condition
	(target text[], condition pgroonga_full_text_search_condition)
	COST 100;
ALTER FUNCTION pgroonga_match_text_array_condition_with_scorers
	(target text[],
	 condition pgroonga_full_text_search_condition_with_scorers)
	COST 100;
ALTER FUNCTION pgroonga_match_varchar(varchar, varchar)
	COST 100;
ALTER FUNCTION pgroonga_match_varchar_condition
	(target varchar, condition pgroonga_full_text_search_condition)
	COST 100;
ALTER FUNCTION pgroonga_match_varchar_condition_with_scorers
	(target varchar,
	 condition pgroonga_full_text_search_condition_with_scorers)
	COST 100;
ALTER FUNCTION pgroonga_contain_varchar_array(varchar[], varchar)
	COST 100;
ALTER FUNCTION pgroonga_query_text(text, text)
	COST 100;
ALTER FUNCTION pgroonga_query_text_condition
	(target text, condition pgroonga_full_text_search_condition)
	COST 100;
ALTER FUNCTION pgroonga_query_text_condition_with_scorers
	(target text,
	 condition pgroonga_full_text_search_condition_with_scorers)
	COST 100;
ALTER FUNCTION pgroonga_query_text_array(text[], text)
	COST 100;
ALTER FUNCTION pgroonga_query_text_array_condition
	(targets text[], condition pgroonga_full_text_search_condition)
	COST 100;
ALTER FUNCTION pgroonga_query_text_array_condition_with_scorers
	(targets text[],
	 condition pgroonga_full_text_search_condition_with_scorers)
	COST 100;
ALTER FUNCTION pgroonga_query_varchar(varchar, varchar)
	COST 100;
ALTER FUNCTION pgroonga_query_varchar_condition
	(target varchar, condition pgroonga_full_text_search_condition)
	COST 100;
ALTER FUNCTION pgroonga_query_varchar_condition_with_scorers
	(target varchar,
	 condition pgroonga_full_text_search_condition_with_scorers)
	COST 100;
ALTER FUNCTION pgroonga_similar_text(text, text)
	COST 100;
ALTER FUNCTION pgroonga_similar_text_array(text[], text)
	COST 100;
ALTER FUNCTION pgroonga_similar_varchar(varchar, varchar)
	COST 100;
ALTER FUNCTION pgroonga_prefix_text(text, text)
	COST 100;
ALTER FUNCTION pgroonga_prefix_text_array(text[], text)
	COST 100;
ALTER FUNCTION pgroonga_prefix_varchar(varchar, varchar)
	COST 100;
ALTER FUNCTION pgroonga_prefix_varchar_array(varchar[], varchar)
	COST 100;
ALTER FUNCTION pgroonga_prefix_rk_text(text, text)
	COST 100;
ALTER FUNCTION pgroonga_prefix_rk_text_array(text[], text)
	COST 100;
ALTER FUNCTION pgroonga_prefix_rk_varchar(varchar, varchar)
	COST 100;
ALTER FUNCTION pgroonga_prefix_rk_varchar_array(varchar[], varchar)
	COST 100;
ALTER FUNCTION pgroonga_script_text(text, text)
	COST 100;
ALTER FUNCTION pgroonga_script_text_array(text[], text)
	COST 100;
ALTER FUNCTION pgroonga_script_varchar(varchar, varchar)
	COST 100;
ALTER FUNCTION pgroonga_match_in_text(text, text[])
	COST 100;
ALTER FUNCTION pgroonga_match_in_text_array(text[], text[])
	COST 100;
ALTER FUNCTION pgroonga_match_in_varchar(varchar, varchar[])
	COST 100;
ALTER FUNCTION pgroonga_query_in_text(text, text[])
	COST 100;
ALTER FUNCTION pgroonga_query_in_text_array(text[], text[])
	COST 100;
ALTER FUNCTION pgroonga_query_in_varchar(varchar, varchar[])
	COST 100;
ALTER FUNCTION pgroonga_prefix_in_text(text, text[])
	COST 100;
ALTER FUNCTION pgroonga_prefix_in_text_array(text[], text[])
	COST 100;
ALTER FUNCTION pgroonga_prefix_in_varchar(varchar, varchar[])
	COST 100;
ALTER FUNCTION pgroonga_prefix_in_varchar_array(varchar[], varchar[])
	COST 100;
ALTER FUNCTION pgroonga_prefix_rk_in_text(text, text[])
	COST 100;
ALTER FUNCTION pgroonga_prefix_rk_in_text_array(text[], text[])
	COST 100;
ALTER FUNCTION pgroonga_prefix_rk_in_varchar(varchar, varchar[])
	COST 100;
ALTER FUNCTION pgroonga_prefix_rk_in_varchar_array(varchar[], varchar[])
	COST 100;
ALTER FUNCTION pgroonga_regexp_text(text, text)
	COST 100;
ALTER FUNCTION pgroonga_regexp_varchar(varchar, varchar)
	COST 100;

ALTER FUNCTION pgroonga_match_jsonb(jsonb, text)
	COST 100;
ALTER FUNCTION pgroonga_query_jsonb(jsonb, text)
	COST 100;
ALTER FUNCTION pgroonga_script_jsonb(jsonb, text)
	COST 100;
