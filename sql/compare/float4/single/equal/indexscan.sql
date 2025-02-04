CREATE TABLE ids (
  id real
);

INSERT INTO ids VALUES (1.1);
INSERT INTO ids VALUES (2.1);
INSERT INTO ids VALUES (3.1);

CREATE INDEX grnindex ON ids USING pgroonga (id pgroonga_float4_ops);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT id
  FROM ids
 WHERE id = (2.1::real);

SELECT id
  FROM ids
 WHERE id = (2.1::real);

DROP TABLE ids;
