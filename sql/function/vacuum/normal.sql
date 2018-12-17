CREATE TABLE memos (
  content text
);

CREATE INDEX pgroonga_index ON memos USING PGroonga (content);

SELECT pgroonga_table_name('pgroonga_index');
\gset old_
REINDEX INDEX pgroonga_index;

SELECT pgroonga_command('select',
                        ARRAY[
                          'table', :'old_pgroonga_table_name'
                        ])::json#>>'{0, 0}';
SELECT pgroonga_vacuum();
SELECT pgroonga_command('select',
                        ARRAY[
                          'table', :'old_pgroonga_table_name'
                        ])::json#>>'{0, 0}';

DROP TABLE memos;
