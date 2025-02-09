SET pgroonga.enable_wal = yes;

CREATE TABLE memos (
  content text
);

INSERT INTO memos VALUES ('Groonga is fast!');

CREATE INDEX pgrn_index ON memos USING PGroonga (content);

INSERT INTO memos VALUES ('PGroonga is also fast!');
DELETE FROM memos;

SET enable_seqscan = false;
SET enable_indexscan = true;
SET enable_bitmapscan = false;

SELECT * FROM memos WHERE content &@~ 'Groonga';
SELECT pgroonga_command('select',
			ARRAY[
			  'table', pgroonga_table_name('pgrn_index'),
			  'output_columns', '_id, content'
			])::jsonb->>1;

SELECT pgroonga_wal_set_applied_position('pgrn_index', 0, 0);
SELECT pgroonga_command('table_remove',
                        ARRAY[
                          'name', 'Lexicon' ||
                                  'pgrn_index'::regclass::oid ||
                                  '_0'
                        ])::jsonb->>1;
SELECT pgroonga_command('table_remove',
                        ARRAY[
                          'name', pgroonga_table_name('pgrn_index')
                        ])::jsonb->>1;

SELECT pgroonga_wal_apply('pgrn_index');

SELECT pgroonga_command('select',
                        ARRAY[
                          'table', pgroonga_table_name('pgrn_index'),
                          'output_columns', '_id, content'
                        ])::jsonb->>1;

DROP TABLE memos;

SET pgroonga.enable_wal = default;
