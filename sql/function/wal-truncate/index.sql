SET pgroonga.enable_wal = yes;

CREATE TABLE memos (
  content text
);

INSERT INTO memos VALUES ('Groonga is fast!');

CREATE INDEX pgrn_index ON memos USING PGroonga (content);

INSERT INTO memos VALUES ('PGroonga is also fast!');

SELECT pgroonga_command('truncate',
                        ARRAY[
                          'table', pgroonga_table_name('pgrn_index')
                        ])::jsonb->>1;

SELECT pgroonga_wal_truncate('pgrn_index');

INSERT INTO memos VALUES ('PostgreSQL is a good RDBMS!');

SELECT pgroonga_command('delete',
                        ARRAY[
                          'table', 'IndexStatuses',
                          'key', 'pgrn_index'::regclass::oid::text
                        ])::jsonb->>1;
SELECT pgroonga_command('truncate',
                        ARRAY[
                          'table', pgroonga_table_name('pgrn_index')
                        ])::jsonb->>1;

SELECT pgroonga_wal_apply('pgrn_index');

SELECT pgroonga_command('select',
                        ARRAY[
                          'table', pgroonga_table_name('pgrn_index'),
                          'output_columns', '_id, content'
                        ])::jsonb->>1;

DROP TABLE memos;
