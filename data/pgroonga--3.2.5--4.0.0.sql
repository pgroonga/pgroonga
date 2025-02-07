-- Upgrade SQL

DROP OPERATOR FAMILY pgroonga.text_full_text_search_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.text_array_full_text_search_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.varchar_full_text_search_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.varchar_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.varchar_array_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.bool_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.int2_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.int4_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.int8_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.float4_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.float8_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.timestamp_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.timestamptz_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.jsonb_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.text_regexp_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.varchar_regexp_ops USING pgroonga;
DROP OPERATOR FAMILY pgroonga.text_full_text_search_ops_v2 USING pgroonga;
DROP OPERATOR FAMILY pgroonga.text_array_full_text_search_ops_v2 USING pgroonga;
DROP OPERATOR FAMILY pgroonga.text_term_search_ops_v2 USING pgroonga;
DROP OPERATOR FAMILY pgroonga.text_array_term_search_ops_v2 USING pgroonga;
DROP OPERATOR FAMILY pgroonga.text_regexp_ops_v2 USING pgroonga;
DROP OPERATOR FAMILY pgroonga.varchar_full_text_search_ops_v2 USING pgroonga;
DROP OPERATOR FAMILY pgroonga.varchar_array_term_search_ops_v2 USING pgroonga;
DROP OPERATOR FAMILY pgroonga.varchar_regexp_ops_v2 USING pgroonga;
DROP OPERATOR FAMILY pgroonga.jsonb_ops_v2 USING pgroonga;

DROP FUNCTION pgroonga.score("row" record);
DROP FUNCTION pgroonga.table_name(indexName cstring);
DROP FUNCTION pgroonga.command(groongaCommand text);
DROP FUNCTION pgroonga.command(groongaCommand text, arguments text[]);
DROP FUNCTION pgroonga.query_expand(tableName cstring,
                                    termColumnName text,
                                    synonymsColumnName text,
                                    query text);
DROP FUNCTION pgroonga.snippet_html(target text, keywords text[], width integer);
DROP FUNCTION pgroonga.highlight_html(target text, keywords text[]);
DROP FUNCTION pgroonga.match_positions_byte(target text, keywords text[]);
DROP FUNCTION pgroonga.match_positions_character(target text, keywords text[]);
DROP FUNCTION pgroonga.query_extract_keywords(query text);
DROP FUNCTION pgroonga.flush(indexName cstring);
DROP FUNCTION pgroonga.command_escape_value(value text);
DROP FUNCTION pgroonga.query_escape(query text);
DROP FUNCTION pgroonga.escape(value text);
DROP FUNCTION pgroonga.escape(value text, special_characters text);
DROP FUNCTION pgroonga.escape(value boolean);
DROP FUNCTION pgroonga.escape(value int2);
DROP FUNCTION pgroonga.escape(value int4);
DROP FUNCTION pgroonga.escape(value int8);
DROP FUNCTION pgroonga.escape(value float4);
DROP FUNCTION pgroonga.escape(value float8);
DROP FUNCTION pgroonga.escape(value timestamp);
DROP FUNCTION pgroonga.escape(value timestamptz);
/* v1 */
DROP FUNCTION pgroonga.match_term(target text, term text);
DROP FUNCTION pgroonga.match_term(target text[], term text);
DROP FUNCTION pgroonga.match_term(target varchar, term varchar);
DROP FUNCTION pgroonga.match_term(target varchar[], term varchar);
DROP FUNCTION pgroonga.match_query(text, text);
DROP FUNCTION pgroonga.match_query(text[], text);
DROP FUNCTION pgroonga.match_query(varchar, varchar);
DROP FUNCTION pgroonga.match_regexp(text, text);
DROP FUNCTION pgroonga.match_regexp(varchar, varchar);
/* v2 */
DROP FUNCTION pgroonga.match_text(text, text);
DROP FUNCTION pgroonga.match_text_array(text[], text);
DROP FUNCTION pgroonga.match_varchar(varchar, varchar);
DROP FUNCTION pgroonga.contain_varchar_array(varchar[], varchar);
DROP FUNCTION pgroonga.match_jsonb(jsonb, text);
DROP FUNCTION pgroonga.query_text(text, text);
DROP FUNCTION pgroonga.query_text_array(text[], text);
DROP FUNCTION pgroonga.query_jsonb(jsonb, text);
DROP FUNCTION pgroonga.similar_text(text, text);
DROP FUNCTION pgroonga.similar_text_array(text[], text);
DROP FUNCTION pgroonga.similar_varchar(varchar, varchar);
DROP FUNCTION pgroonga.prefix_text(text, text);
DROP FUNCTION pgroonga.prefix_text_array(text[], text);
DROP FUNCTION pgroonga.prefix_rk_text(text, text);
DROP FUNCTION pgroonga.prefix_rk_text_array(text[], text);
DROP FUNCTION pgroonga.script_text(text, text);
DROP FUNCTION pgroonga.script_text_array(text[], text);
DROP FUNCTION pgroonga.script_varchar(varchar, varchar);
DROP FUNCTION pgroonga.script_jsonb(jsonb, text);
DROP FUNCTION pgroonga.match_in_text(text, text[]);
DROP FUNCTION pgroonga.match_in_text_array(text[], text[]);
DROP FUNCTION pgroonga.match_in_varchar(varchar, varchar[]);
DROP FUNCTION pgroonga.query_in_text(text, text[]);
DROP FUNCTION pgroonga.query_in_text_array(text[], text[]);
DROP FUNCTION pgroonga.query_in_varchar(varchar, varchar[]);
DROP FUNCTION pgroonga.prefix_in_text(text, text[]);
DROP FUNCTION pgroonga.prefix_in_text_array(text[], text[]);
DROP FUNCTION pgroonga.prefix_rk_in_text(text, text[]);
DROP FUNCTION pgroonga.prefix_rk_in_text_array(text[], text[]);
DROP FUNCTION pgroonga.regexp_text(text, text);
DROP FUNCTION pgroonga.regexp_varchar(varchar, varchar);
DROP FUNCTION pgroonga.match_script_jsonb(jsonb, text);

DROP SCHEMA pgroonga;
