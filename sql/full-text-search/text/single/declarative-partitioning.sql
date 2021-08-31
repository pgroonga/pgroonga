CREATE TABLE memos (
  id integer,
  content text
) PARTITION BY RANGE (id);

CREATE TABLE memos_0_1000
  PARTITION OF memos
  FOR VALUES FROM (0) TO (1000);
CREATE TABLE memos_1000_2000
  PARTITION OF memos
  FOR VALUES FROM (1000) TO (2000);
CREATE TABLE memos_2000_3000
  PARTITION OF memos
  FOR VALUES FROM (2000) TO (3000);

INSERT INTO memos
  SELECT id, 'data: ' || id
    FROM generate_series(0, 2999) AS id;

CREATE INDEX memos_fts ON memos USING pgroonga (content);

SET parallel_setup_cost = 0;
SET parallel_tuple_cost = 0;
SET min_parallel_table_scan_size = 0;
SET max_parallel_workers_per_gather = 4;
SET force_parallel_mode = on;
SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT count(id)
  FROM memos
 WHERE content &@~ 'data 9*';

SELECT count(id)
  FROM memos
 WHERE content &@~ 'data 9*';

DROP TABLE memos;
