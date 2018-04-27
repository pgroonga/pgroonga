CREATE TABLE memos (
  created_at timestamp with time zone
);

INSERT INTO memos VALUES ('2018-02-02+09:00');

CREATE INDEX pgroonga_index ON memos USING pgroonga (created_at);

SELECT pgroonga_command('select',
                        ARRAY['table', pgroonga_table_name('pgroonga_index'),
                              'output_columns', 'created_at'])::json->>1
    AS body;

SET enable_seqscan = off;
SET enable_bitmapscan = off;
SET enable_indexscan = on;
SET enable_indexonlyscan = on;

EXPLAIN (COSTS OFF)
SELECT *
  FROM memos
 WHERE created_at >= '2018-02-02+09:00';

SELECT *
  FROM memos
 WHERE created_at >= '2018-02-02+09:00';

DROP TABLE memos;
