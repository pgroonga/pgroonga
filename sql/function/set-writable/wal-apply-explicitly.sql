SET pgroonga.enable_wal = true;

CREATE TABLE memos (
  content text
);

INSERT INTO memos VALUES ('Groonga is fast!');

CREATE INDEX pgrn_index ON memos USING PGroonga (content);

SELECT pgroonga_command('object_exist',
                        ARRAY[
                          'name', 'IndexStatuses'
                        ])::jsonb->>1;
SELECT pgroonga_command('delete',
                        ARRAY[
                          'table', 'IndexStatuses',
                          'key', 'pgrn_index'::regclass::oid::text
                        ])::jsonb->>1;
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

SELECT pgroonga_set_writable(false);

SELECT pgroonga_wal_apply('pgrn_index');
SELECT pgroonga_command('object_exist',
			ARRAY[
			  'name', pgroonga_table_name('pgrn_index')
			])::jsonb->>1;

SELECT pgroonga_set_writable(true);

SELECT pgroonga_wal_apply('pgrn_index');
SELECT pgroonga_command('select',
			ARRAY[
			  'table', pgroonga_table_name('pgrn_index'),
			  'output_columns', '_id, content'
			])::jsonb->>1;

DROP TABLE memos;

SET pgroonga.enable_wal = default;
