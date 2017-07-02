CREATE TABLE memos (
  id integer,
  content text
);

INSERT INTO memos VALUES (1, 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES (3, 'PGroonga is a PostgreSQL extension that uses Groonga.');

CREATE INDEX grnindex ON memos
 USING pgroonga (content pgroonga.text_full_text_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT id, content
  FROM memos
 WHERE content &@* 'Mroonga is a MySQL plugin that uses Groonga.';

SELECT id, content
  FROM memos
 WHERE content &@* 'Mroonga is a MySQL plugin that uses Groonga.';

DROP TABLE memos;
