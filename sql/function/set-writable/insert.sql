CREATE TABLE memos (
  content text
);

CREATE INDEX pgrn_index ON memos USING PGroonga (content);

SELECT pgroonga_set_writable(false);

INSERT INTO memos VALUES ('Groonga is fast!');

SELECT * FROM memos;

SELECT pgroonga_set_writable(true);

INSERT INTO memos VALUES ('Groonga is fast!');

SELECT * FROM memos;
SELECT pgroonga_command('select',
			ARRAY[
			  'table', pgroonga_table_name('pgrn_index'),
			  'output_columns', '_id, content'
			])::jsonb->>1;

DROP TABLE memos;
