CREATE TABLE logs (
  message jsonb
);

CREATE INDEX pgroonga_index ON logs USING PGroonga (message);

SELECT pgroonga_table_name('pgroonga_index')
\gset old_
REINDEX INDEX pgroonga_index;

SELECT pgroonga_command('object_exist',
                        ARRAY[
                          'name', :'old_pgroonga_table_name'
                        ])::json->>1;
SELECT pgroonga_vacuum();
SELECT pgroonga_command('object_exist',
                        ARRAY[
                          'name', :'old_pgroonga_table_name'
                        ])::json->>1;

DROP TABLE logs;
