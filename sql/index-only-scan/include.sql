CREATE TABLE ids (
  id integer NOT NULL,
  memo text NOT NULL
);

CREATE INDEX pgroonga_index ON ids USING pgroonga (id) INCLUDE (memo);

INSERT INTO ids VALUES (1, 'a');

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT * FROM ids WHERE id > 0;

SELECT * FROM ids WHERE id > 0;

DROP TABLE ids;
