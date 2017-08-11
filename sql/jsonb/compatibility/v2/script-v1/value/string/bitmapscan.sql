CREATE TABLE fruits (
  id int,
  items jsonb
);

INSERT INTO fruits VALUES (1, '{"name": "apple"}');
INSERT INTO fruits VALUES (2, '{"type": "apple"}');
INSERT INTO fruits VALUES (3, '{"name": "peach"}');

CREATE INDEX pgroonga_index ON fruits
  USING pgroonga (items pgroonga_jsonb_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

SELECT id, items
  FROM fruits
 WHERE items @@ 'string == "apple"'
 ORDER BY id;

DROP TABLE fruits;
