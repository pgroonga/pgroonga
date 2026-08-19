CREATE TABLE memos (
  id integer,
  content text
);

INSERT INTO memos VALUES (1, 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES (3, 'PGroonga is a PostgreSQL extension that uses Groonga.');

CREATE INDEX pgrn_content_index ON memos
  USING pgroonga (content);

SELECT idx_scan
  FROM pg_stat_user_indexes
 WHERE indexrelname = 'pgrn_content_index';

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT id, content
  FROM memos
 WHERE content &@~ 'Groonga';

SELECT id, content
  FROM memos
 WHERE content &@~ 'Groonga';

SELECT pg_stat_force_next_flush();

SELECT idx_scan
  FROM pg_stat_user_indexes
 WHERE indexrelname = 'pgrn_content_index';

DROP TABLE memos;
