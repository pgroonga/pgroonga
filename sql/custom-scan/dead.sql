CREATE TABLE memos (
  id integer PRIMARY KEY,
  content text
);

CREATE INDEX grnindex ON memos USING pgroonga (id, content);

INSERT INTO memos VALUES (1, 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES (3, 'PGroonga is a PostgreSQL extension that uses Groonga.');
UPDATE memos
   SET content = 'PGroonga is a PostgreSQL extension that uses Groonga!!!'
 WHERE id = 3;

SET pgroonga.enable_custom_scan = on;

EXPLAIN (COSTS OFF)
SELECT id, content
  FROM memos
 WHERE content &@~ 'PGroonga OR Groonga';

SELECT id, content
  FROM memos
 WHERE content &@~ 'PGroonga OR Groonga';

DROP TABLE memos;
