CREATE TABLE memos (
  id integer,
  content text
);

INSERT INTO memos VALUES (1, 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'Groonga is (fast) full text search engine.');
INSERT INTO memos VALUES (3, 'PGroonga is a PostgreSQL extension that uses Groonga.');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (id, content pgroonga_text_full_text_search_ops_v2)
  WITH (query_allow_column = true);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT id, content
  FROM memos
 WHERE content &@~
       pgroonga_condition('id:1 PostgreSQL', index_name => 'pgrn_index')
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g"
\pset format aligned

SELECT id, content
  FROM memos
 WHERE content &@~
       pgroonga_condition('id:1 PostgreSQL', index_name => 'pgrn_index');

DROP TABLE memos;
