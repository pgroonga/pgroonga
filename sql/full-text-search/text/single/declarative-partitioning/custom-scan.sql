CREATE TABLE memos (
  id integer,
  content text
) PARTITION BY RANGE (id);

CREATE TABLE memos_0_10000
  PARTITION OF memos
  FOR VALUES FROM (0) TO (10000);
CREATE TABLE memos_10000_20000
  PARTITION OF memos
  FOR VALUES FROM (10000) TO (20000);
CREATE TABLE memos_20000_30000
  PARTITION OF memos
  FOR VALUES FROM (20000) TO (30000);

INSERT INTO memos
  SELECT id, 'data: ' || id
    FROM generate_series(0, 29999) AS id;

CREATE INDEX memos_fts ON memos USING pgroonga (content);

SET parallel_setup_cost = 0;
SET parallel_tuple_cost = 0;
SET min_parallel_table_scan_size = 0;
SET max_parallel_workers_per_gather = 4;
DO LANGUAGE plpgsql $$
BEGIN
        PERFORM 1
                WHERE current_setting('server_version_num')::int >= 160000;
        IF FOUND THEN
                SET debug_parallel_query = on;
        ELSE
                SET force_parallel_mode = on;
        END IF;
END;
$$;

SET pgroonga.enable_custom_scan = on;

/*
 * todo Support for parallel scans.
 * Supporting parallel scans should change the results of EXPALIN.
 */
EXPLAIN (COSTS OFF)
SELECT count(id)
  FROM memos
 WHERE content &@~ 'data 9*';

SELECT count(id)
  FROM memos
 WHERE content &@~ 'data 9*';

DROP TABLE memos;
