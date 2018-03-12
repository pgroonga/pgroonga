CREATE TABLE memos (
  id integer PRIMARY KEY,
  tag varchar(256),
  content text
);

CREATE INDEX pgrn_index ON memos USING pgroonga (content);

INSERT INTO memos VALUES (1, 'pgsql', 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'groonga', 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES (3, 'pgsql', 'PGroonga is a PostgreSQL extension that uses Groonga.');
UPDATE memos SET tag = 'groonga'
 WHERE id = 3;

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT id, content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@~ 'PGroonga OR Groonga';

SELECT id, content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@~ 'PGroonga OR Groonga';

DROP TABLE memos;
