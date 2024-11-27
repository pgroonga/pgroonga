CREATE TABLE memos (
  content text
);

INSERT INTO memos VALUES ('PostgreSQL is a RDBMS');
INSERT INTO memos VALUES ('Groonga is fast full text search engine');
INSERT INTO memos VALUES ('PGroonga is a PostgreSQL extension that uses Groonga');

CREATE INDEX pgrn_content_index ON memos
 USING pgroonga (content pgroonga_text_regexp_ops);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT *
  FROM memos
 WHERE content @~ '';

SELECT *
  FROM memos
 WHERE content @~ '';

DROP TABLE memos;
