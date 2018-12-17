CREATE TABLE memos (
  id integer,
  content varchar(256)
);

INSERT INTO memos VALUES (1, 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES (3, 'PGroonga is a PostgreSQL extension that uses Groonga.');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (content pgroonga_varchar_full_text_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT id, content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@
       ('Groonga',
        ARRAY[5],
        ARRAY['scorer_tf_at_most($index, 0.25)'],
        'pgrn_index')::pgroonga_full_text_search_condition_with_scorers
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g"
\pset format aligned

SELECT id, content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@
       ('Groonga',
        ARRAY[5],
        ARRAY['scorer_tf_at_most($index, 0.25)'],
        'pgrn_index')::pgroonga_full_text_search_condition_with_scorers;

DROP TABLE memos;
