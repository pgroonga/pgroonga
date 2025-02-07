CREATE TABLE ids (
  id real
);

INSERT INTO ids VALUES (1.1);
INSERT INTO ids VALUES (2.1);
INSERT INTO ids VALUES (3.1);

CREATE INDEX pgroonga_index ON ids USING pgroonga (id pgroonga_float4_ops);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT id
  FROM ids
 WHERE id = ANY(ARRAY[]::real[])
 ORDER BY id ASC;

SELECT id
  FROM ids
 WHERE id = ANY(ARRAY[]::real[])
 ORDER BY id ASC;

DROP TABLE ids;
