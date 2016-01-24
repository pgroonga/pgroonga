CREATE TABLE fruits (
  id int,
  items jsonb
);

INSERT INTO fruits VALUES (1, '{"apple":  true}');
INSERT INTO fruits VALUES (2, '{"banana": false}');
INSERT INTO fruits VALUES (3, '{"peach":  true}');

CREATE INDEX pgroonga_index ON fruits USING pgroonga (items);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SELECT id, items
  FROM fruits
 WHERE items @@ 'boolean == true'
 ORDER BY id;

DROP TABLE fruits;
