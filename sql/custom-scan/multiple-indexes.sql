CREATE TABLE memos (
  id integer PRIMARY KEY,
  tag varchar(256),
  content text
);

CREATE INDEX tag_index ON memos USING pgroonga (tag);
CREATE INDEX content_index ON memos USING pgroonga (content);

INSERT INTO memos VALUES (1, 'pgsql', 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'groonga', 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES (3, 'pgroonga', 'PGroonga is a PostgreSQL extension that uses Groonga.');

SET pgroonga.enable_custom_scan = on;

EXPLAIN (COSTS OFF)
SELECT id, tag, content
  FROM memos
 WHERE content &@~ 'PGroonga OR Groonga';

SELECT id, tag, content
  FROM memos
 WHERE content &@~ 'PGroonga OR Groonga';

DROP TABLE memos;
