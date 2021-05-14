CREATE TABLE logs (
  record jsonb,
  group_id int
);

CREATE INDEX pgroonga_index ON logs
  USING pgroonga ((record->'host'))
  WHERE group_id = 1;

INSERT INTO logs VALUES ('{"host": "www"}',      1);
INSERT INTO logs VALUES ('{"message": "error"}', 1);
INSERT INTO logs VALUES ('{"host": "www"}',      2);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT * FROM logs WHERE group_id = 1;

SELECT * FROM logs WHERE group_id = 1;

DROP TABLE logs;
