CREATE TABLE ids (
  id integer
);

INSERT INTO ids VALUES (1);
INSERT INTO ids VALUES (2);
INSERT INTO ids VALUES (3);

CREATE INDEX pgroonga_index ON ids USING pgroonga (id);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT id
  FROM ids
 WHERE id = ANY(ARRAY[]::integer[])
 ORDER BY id ASC;

SELECT id
  FROM ids
 WHERE id = ANY(ARRAY[]::integer[])
 ORDER BY id ASC;

DROP TABLE ids;
