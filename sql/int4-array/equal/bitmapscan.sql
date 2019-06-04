CREATE TABLE records (
  ids int4[]
);

INSERT INTO records VALUES (ARRAY[1, 2]);
INSERT INTO records VALUES (ARRAY[2, 3, 4]);
INSERT INTO records VALUES (ARRAY[3, 4, 5]);

CREATE INDEX pgroonga_index ON records USING pgroonga (ids);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT ids
  FROM records
 WHERE ids = ARRAY[2, 3, 4];

SELECT ids
  FROM records
 WHERE ids = ARRAY[2, 3, 4];

DROP TABLE records;
