CREATE TABLE memos (
  content text
);

INSERT INTO memos VALUES ('PostgreSQL is a RDBMS');

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT *
  FROM memos
 WHERE content &~| ARRAY[''];

SELECT *
  FROM memos
 WHERE content &~| ARRAY[''];

DROP TABLE memos;
