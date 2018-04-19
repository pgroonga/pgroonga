CREATE TABLE memos (
  id integer,
  content text
);

INSERT INTO memos VALUES (1, 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES (3, 'PGroonga is a PostgreSQL extension that uses Groonga.');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (content pgroonga_text_full_text_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT id, content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@~ ROW('Groonga OR PostgreSQL -PGroonga', ARRAY[0], 'pgrn_index');

SELECT id, content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@~ ROW('Groonga OR PostgreSQL -PGroonga', ARRAY[0], 'pgrn_index');

DROP TABLE memos;
