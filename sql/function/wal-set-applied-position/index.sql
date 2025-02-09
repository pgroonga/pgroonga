SET pgroonga.enable_wal = yes;

CREATE TABLE memos (
  content text
);

INSERT INTO memos VALUES ('Groonga is fast!');

CREATE INDEX pgrn_index ON memos USING PGroonga (content);

SELECT current_block, current_offset FROM pgroonga_wal_status() \gset
INSERT INTO memos VALUES ('PGroonga is also fast!');
SELECT pgroonga_wal_set_applied_position('pgrn_index', :current_block, :current_offset);

SELECT pgroonga_command('delete',
                        ARRAY[
                          'table', pgroonga_table_name('pgrn_index'),
                          'filter', 'true'
                        ])::jsonb->>1;
SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;
EXPLAIN (COSTS OFF)
SELECT *
  FROM memos
 WHERE content &@~ 'is';
SELECT *
  FROM memos
 WHERE content &@~ 'is';

DROP TABLE memos;

SET pgroonga.enable_wal = default;
