CREATE TABLE memos (
  content varchar(256)
);

INSERT INTO memos VALUES ('PostgreSQL is a RDBMS');
INSERT INTO memos VALUES ('Groonga is fast full text search engine');
INSERT INTO memos VALUES ('PGroonga is a PostgreSQL extension that uses Groonga');

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT *
  FROM memos
 WHERE content &~| ARRAY['']::varchar[];

SELECT *
  FROM memos
 WHERE content &~| ARRAY['']::varchar[];

DROP TABLE memos;
