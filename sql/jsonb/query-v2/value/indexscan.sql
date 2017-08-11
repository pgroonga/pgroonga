CREATE TABLE fruits (
  id int,
  items jsonb
);

INSERT INTO fruits VALUES (1, '{"name": "apple"}');
INSERT INTO fruits VALUES (2, '{"type": "apple"}');
INSERT INTO fruits VALUES (3, '{"name": "peach"}');
INSERT INTO fruits VALUES (4, '{"like": "banana"}');

CREATE INDEX pgroonga_index ON fruits
  USING pgroonga (items pgroonga_jsonb_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT id, items
  FROM fruits
 WHERE items &@~ 'apple OR banana'
 ORDER BY id;

SELECT id, items
  FROM fruits
 WHERE items &@~ 'apple OR banana'
 ORDER BY id;

DROP TABLE fruits;
