CREATE TABLE memos (
  id integer,
  content text
);

INSERT INTO memos VALUES (1, 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES (3, 'PGroonga is a PostgreSQL extension that uses Groonga.');

CREATE INDEX pgrn_content_index ON memos
  USING pgroonga (content);

SELECT idx_scan
  FROM pg_stat_user_indexes
 WHERE indexrelname = 'pgrn_content_index';

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT id, content
  FROM memos
 WHERE content &@~ 'Groonga';

SELECT id, content
  FROM memos
 WHERE content &@~ 'Groonga';

DO LANGUAGE plpgsql $$
BEGIN
        IF current_setting('server_version_num')::int >= 150000 THEN
                PERFORM pg_stat_force_next_flush();
        ELSE
                PERFORM pg_sleep(1);
        END IF;
END;
$$;

SELECT idx_scan
  FROM pg_stat_user_indexes
 WHERE indexrelname = 'pgrn_content_index';

DROP TABLE memos;
