CREATE TABLE memos (
  content text
);

CREATE INDEX pgrn_index ON memos USING PGroonga (content);

INSERT INTO memos VALUES ('Groonga is fast!');
DELETE FROM memos;

SET pgroonga.writable = false;

SET enable_seqscan = false;
SET enable_indexscan = true;
SET enable_bitmapscan = false;

SELECT * FROM memos WHERE content &@~ 'Groonga';
SELECT pgroonga_command('select',
			ARRAY[
			  'table', pgroonga_table_name('pgrn_index'),
			  'output_columns', '_id, content'
			])::jsonb->>1;

SET pgroonga.writable = true;

SELECT * FROM memos WHERE content &@~ 'Groonga';
SELECT pgroonga_command('select',
			ARRAY[
			  'table', pgroonga_table_name('pgrn_index'),
			  'output_columns', '_id, content'
			])::jsonb->>1;

DROP TABLE memos;
