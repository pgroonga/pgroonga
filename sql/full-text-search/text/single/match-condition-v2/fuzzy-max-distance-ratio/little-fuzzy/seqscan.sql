CREATE TABLE memos (
  id integer,
  content text
);

INSERT INTO memos VALUES (1, 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES (3, 'PGroonga is a PostgreSQL extension that uses Groonga.');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (content pgroonga_text_full_text_search_ops_v2);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT id, content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@~
         pgroonga_condition(
           'PProonga',
           index_name => 'pgrn_index',
           fuzzy_max_distance_ratio => 0.2
         )
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g"
\pset format aligned

SELECT id, content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@~
         pgroonga_condition(
           'PProonga',
           index_name => 'pgrn_index',
           fuzzy_max_distance_ratio => 0.2
         );

DROP TABLE memos;
