CREATE TABLE memos (
  content text
);

CREATE INDEX pgrn_index ON memos USING PGroonga (content);

INSERT INTO memos VALUES ('Groonga is fast!');
DELETE FROM memos;

SELECT pgroonga_set_writable(false);

SET enable_seqscan = false;
SET enable_indexscan = true;
SET enable_bitmapscan = false;

SELECT * FROM memos WHERE content &@~ 'Groonga';
SELECT pgroonga_command('select',
			ARRAY[
			  'table', pgroonga_table_name('pgrn_index'),
			  'output_columns', '_id, content'
			])::jsonb->>1;

SELECT pgroonga_set_writable(true);

SELECT * FROM memos WHERE content &@~ 'Groonga';
SELECT pgroonga_command('select',
			ARRAY[
			  'table', pgroonga_table_name('pgrn_index'),
			  'output_columns', '_id, content'
			])::jsonb->>1;

DROP TABLE memos;
