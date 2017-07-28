CREATE TABLE memos (
  id text PRIMARY KEY,
  content text
);

CREATE INDEX pgroonga_index ON memos
 USING pgroonga (id pgroonga.text_term_search_ops_v2,
                 content);

INSERT INTO memos VALUES ('a', 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES ('b', 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES ('c', 'PGroonga is a PostgreSQL extension that uses Groonga.');

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT id, content, pgroonga.score(memos)
  FROM memos
 WHERE content &@~ 'PGroonga OR Groonga';

DROP TABLE memos;
