CREATE TABLE memos (
  id integer PRIMARY KEY,
  tag varchar(256),
  content text
);

CREATE INDEX content_index ON memos USING pgroonga (id, content);

INSERT INTO memos VALUES (1, 'pgsql', 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'groonga', 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES (3, 'pgroonga', 'PGroonga is a PostgreSQL extension that uses Groonga.');
INSERT INTO memos VALUES (4, 'groonga', 'I like Groonga.');

SET pgroonga.enable_custom_scan = on;

EXPLAIN (COSTS OFF)
SELECT id, content
  FROM memos
 WHERE content &@~ 'PGroonga OR Groonga' AND id >= 3;

SELECT id, content
  FROM memos
 WHERE content &@~ 'PGroonga OR Groonga' AND id >= 3;

DROP TABLE memos;
