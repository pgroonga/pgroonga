-- Downgrade SQL

DROP FUNCTION IF EXISTS pgroonga_language_model_vectorize;

DROP OPERATOR FAMILY pgroonga_text_semantic_search_ops_v2 USING pgroonga;
DROP OPERATOR &@* (text, pgroonga_condition);
DROP FUNCTION pgroonga_similar_text_condition;
