CREATE TABLE memos (
  id integer,
  content text
);

INSERT INTO memos VALUES (1, 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES (3, 'PGroonga is a PostgreSQL extension that uses Groonga.');

CREATE INDEX grnindex ON memos
 USING pgroonga (content pgroonga_text_full_text_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SHOW pgroonga.force_match_escalation;
EXPLAIN (COSTS OFF)
SELECT id, content
  FROM memos
 WHERE content &@~ 'gro eng';
SELECT id, content
  FROM memos
 WHERE content &@~ 'gro eng';

SET pgroonga.force_match_escalation = on;
SHOW pgroonga.force_match_escalation;
EXPLAIN (COSTS OFF)
SELECT id, content
  FROM memos
 WHERE content &@~ 'gro eng';
SELECT id, content
  FROM memos
 WHERE content &@~ 'gro eng';

SET pgroonga.force_match_escalation = default;

DROP TABLE memos;
