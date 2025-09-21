/* Temporary test to be used during implementation of a custom scan. */

CREATE TABLE memos (content text);
CREATE INDEX memos_content ON memos USING pgroonga (content);
INSERT INTO memos VALUES ('PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES ('Groonga is fast full text search engine.');
INSERT INTO memos VALUES ('PGroonga is a PostgreSQL extension that uses Groonga.');

SET pgroonga.enable_custom_scan = on;

EXPLAIN (COSTS OFF)
SELECT content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@~ 'PGroonga OR Groonga';

SELECT content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@~ 'PGroonga OR Groonga';

DROP TABLE memos;
