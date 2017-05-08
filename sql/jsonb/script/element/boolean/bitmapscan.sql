CREATE TABLE fruits (
  id int,
  items jsonb
);

INSERT INTO fruits VALUES (1, '[true]');
INSERT INTO fruits VALUES (2, '[false]');
INSERT INTO fruits VALUES (3, '[true]');

CREATE INDEX pgroonga_index ON fruits USING pgroonga (items);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

SELECT id, items
  FROM fruits
 WHERE items @@ 'boolean == true'
 ORDER BY id;

DROP TABLE fruits;
