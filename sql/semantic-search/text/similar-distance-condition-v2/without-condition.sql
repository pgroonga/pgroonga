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
INSERT INTO memos VALUES (3, 'Ruby is a object oriented script language.');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (content pgroonga_text_semantic_search_ops_v2)
 WITH (plugins = 'language_model/knn',
       model = 'hf:///groonga/multilingual-e5-base-Q4_K_M-GGUF',
       passage_prefix = 'passage: ',
       query_prefix = 'query: ');

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT id, content
  FROM memos
 ORDER BY content <&@*> pgroonga_condition('What is a MySQL alternative?')
 LIMIT 1;

SELECT id, content
  FROM memos
 ORDER BY content <&@*> pgroonga_condition('What is a MySQL alternative?')
 LIMIT 1;

DROP TABLE memos;
