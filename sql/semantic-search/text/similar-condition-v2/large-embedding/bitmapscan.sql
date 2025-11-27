-- Only test when `PGRN_LANGUAGE_MODEL_TEST` is set.
\getenv language_model_test PGRN_LANGUAGE_MODEL_TEST
SELECT NOT :{?language_model_test} AS omit \gset
\if :omit
  \quit
\endif

CREATE TABLE memos (
  id integer,
  content text
);

INSERT INTO memos VALUES (1, 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'Groonga is fast full text search engine.');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (content pgroonga_text_semantic_search_ops_v2)
 WITH (plugins = 'language_model/knn',
       model = 'hf:///Qwen/Qwen3-Embedding-4B-GGUF',
       n_gpu_layers = 0);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT id, content
  FROM memos
 WHERE content &@* pgroonga_condition('What is a MySQL alternative?')
 ORDER BY pgroonga_score(tableoid, ctid) DESC;

SELECT id, content
  FROM memos
 WHERE content &@* pgroonga_condition('What is a MySQL alternative?')
 ORDER BY pgroonga_score(tableoid, ctid) DESC;

DROP TABLE memos;
