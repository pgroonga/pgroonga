CREATE TABLE memos (
  content text
);

CREATE INDEX pgroonga_index ON memos USING PGroonga (content);

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

DROP TABLE memos;
