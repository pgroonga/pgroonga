SET pgroonga.enable_wal = yes;

CREATE TABLE memos (
  id integer,
  content text
) PARTITION BY RANGE (id);

CREATE TABLE memos_0_10
  PARTITION OF memos
  FOR VALUES FROM (0) TO (10);
CREATE TABLE memos_10_20
  PARTITION OF memos
  FOR VALUES FROM (10) TO (20);
CREATE TABLE memos_20_30
  PARTITION OF memos
  FOR VALUES FROM (20) TO (30);

INSERT INTO memos
  SELECT id, 'data: ' || id
    FROM generate_series(0, 29) AS id;

CREATE INDEX memos_fts ON memos USING pgroonga (content);

SELECT pgroonga_wal_set_applied_position(0, 0);
SELECT name, current_block, current_offset FROM pgroonga_wal_status();
SELECT pgroonga_wal_set_applied_position();

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT count(id)
  FROM memos
 WHERE content &@~ 'data 1*'
\g |sed -r -e "s/ memos_[123]$//g"
\pset format aligned

SELECT count(id)
  FROM memos
 WHERE content &@~ 'data 1*';

DROP TABLE memos;
