CREATE TABLE memos (
  content varchar(256)
);

INSERT INTO memos VALUES ('PostgreSQL is a RDBMS');

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
