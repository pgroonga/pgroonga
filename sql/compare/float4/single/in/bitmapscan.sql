CREATE TABLE ids (
  id real
);

INSERT INTO ids VALUES (2.1);
INSERT INTO ids VALUES (7.1);
INSERT INTO ids VALUES (6.1);
INSERT INTO ids VALUES (4.1);
INSERT INTO ids VALUES (5.1);
INSERT INTO ids VALUES (8.1);
INSERT INTO ids VALUES (1.1);
INSERT INTO ids VALUES (10.1);
INSERT INTO ids VALUES (3.1);
INSERT INTO ids VALUES (9.1);

CREATE INDEX pgroonga_index ON ids USING pgroonga (id pgroonga_float4_ops);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT id
  FROM ids
 WHERE id IN ((6.1::real), (1.1::real), (7.1::real))
 ORDER BY id ASC;

SELECT id
  FROM ids
 WHERE id IN ((6.1::real), (1.1::real), (7.1::real))
 ORDER BY id ASC;

DROP TABLE ids;
