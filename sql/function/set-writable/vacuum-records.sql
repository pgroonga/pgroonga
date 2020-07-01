CREATE TABLE memos (
  content text
);

CREATE INDEX pgrn_index ON memos USING PGroonga (content);

INSERT INTO memos VALUES ('Groonga is fast!');
DELETE FROM memos;

SELECT pgroonga_set_writable(false);

\set SHOW_CONTEXT never
VACUUM memos;
SELECT pgroonga_command('select',
			ARRAY[
			  'table', pgroonga_table_name('pgrn_index'),
			  'output_columns', '_id, content'
			])::jsonb->>1;

SELECT pgroonga_set_writable(true);

INSERT INTO memos VALUES ('PGroonga is fast!');
DELETE FROM memos;

VACUUM memos;
SELECT pgroonga_command('select',
			ARRAY[
			  'table', pgroonga_table_name('pgrn_index'),
			  'output_columns', '_id, content'
			])::jsonb->>1;

DROP TABLE memos;
