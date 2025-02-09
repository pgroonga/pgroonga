SET pgroonga.enable_wal = yes;

CREATE TABLE logs (
  record jsonb
);

INSERT INTO logs VALUES ('{"message": "Groonga is fast!"}');

CREATE INDEX pgrn_index ON logs USING PGroonga (record);

INSERT INTO logs VALUES ('{"message": "PGroonga is also fast!"}');

SELECT pgroonga_wal_set_applied_position('pgrn_index', 0, 0);
SELECT pgroonga_command('table_remove',
                        ARRAY[
                          'name', 'JSONValueLexiconBoolean' ||
                                  'pgrn_index'::regclass::oid ||
                                  '_0'
                        ])::jsonb->>1;
SELECT pgroonga_command('table_remove',
                        ARRAY[
                          'name', 'JSONValueLexiconFullTextSearch' ||
                                  'pgrn_index'::regclass::oid ||
                                  '_0'
                        ])::jsonb->>1;
SELECT pgroonga_command('table_remove',
                        ARRAY[
                          'name', 'JSONValueLexiconNumber' ||
                                  'pgrn_index'::regclass::oid ||
                                  '_0'
                        ])::jsonb->>1;
SELECT pgroonga_command('table_remove',
                        ARRAY[
                          'name', 'JSONValueLexiconSize' ||
                                  'pgrn_index'::regclass::oid ||
                                  '_0'
                        ])::jsonb->>1;
SELECT pgroonga_command('table_remove',
                        ARRAY[
                          'name', 'JSONValueLexiconString' ||
                                  'pgrn_index'::regclass::oid ||
                                  '_0'
                        ])::jsonb->>1;
SELECT pgroonga_command('table_remove',
                        ARRAY[
                          'name', pgroonga_table_name('pgrn_index')
                        ])::jsonb->>1;
SELECT pgroonga_command('table_remove',
                        ARRAY[
                          'name', 'JSONValues' ||
                                  'pgrn_index'::regclass::oid ||
                                  '_0'
                        ])::jsonb->>1;
SELECT pgroonga_command('table_remove',
                        ARRAY[
                          'name', 'JSONPaths' ||
                                  'pgrn_index'::regclass::oid ||
                                  '_0'
                        ])::jsonb->>1;
SELECT pgroonga_command('table_remove',
                        ARRAY[
                          'name', 'JSONTypes' ||
                                  'pgrn_index'::regclass::oid ||
                                  '_0'
                        ])::jsonb->>1;

SELECT pgroonga_wal_apply('pgrn_index');

SELECT pgroonga_command('select',
                        ARRAY[
                          'table', pgroonga_table_name('pgrn_index'),
                          'columns[record_paths].stage', 'output',
                          'columns[record_paths].flags', 'COLUMN_VECTOR',
                          'columns[record_paths].type', 'ShortText',
                          'columns[record_paths].value', 'record.path._key',
                          'output_columns', '_id, record_paths'
                        ])::jsonb->>1;

DROP TABLE logs;

SET pgroonga.enable_wal = default;
