CREATE TABLE ids (
  id integer,
  memo text
);

CREATE INDEX pgroonga_index ON ids USING pgroonga (id, memo);

INSERT INTO ids VALUES (1, 'a');
INSERT INTO ids VALUES (NULL, 'b');
INSERT INTO ids VALUES (3, NULL);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT * FROM ids WHERE id > 0;

DROP TABLE ids;
