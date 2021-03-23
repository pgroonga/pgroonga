CREATE TABLE memos (
  content text NOT NULL
);

CREATE INDEX pgroonga_index ON memos USING pgroonga (content);

INSERT INTO memos
  SELECT string_agg(c, '')
    FROM (
      SELECT chr(65 + (random() * 1000)::int % 57) AS c
        FROM generate_series(1, 9000)
    ) AS data;

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT COUNT(*) FROM memos;

SELECT COUNT(*) FROM memos;

DROP TABLE memos;
