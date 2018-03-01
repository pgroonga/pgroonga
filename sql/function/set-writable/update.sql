CREATE TABLE memos (
  content text
);

CREATE INDEX pgrn_index ON memos USING PGroonga (content);

INSERT INTO memos VALUES ('Groonga is fast!');

SELECT pgroonga_set_writable(false);

UPDATE memos SET content = 'PGroonga is fast!';

SELECT * FROM memos;
SELECT pgroonga_command('select',
			ARRAY[
			  'table', pgroonga_table_name('pgrn_index'),
			  'output_columns', '_id, content'
			])::jsonb->>1;

SELECT pgroonga_set_writable(true);

UPDATE memos SET content = 'PGroonga is fast!';

SELECT * FROM memos;
SELECT pgroonga_command('select',
			ARRAY[
			  'table', pgroonga_table_name('pgrn_index'),
			  'output_columns', '_id, content'
			])::jsonb->>1;

DROP TABLE memos;
