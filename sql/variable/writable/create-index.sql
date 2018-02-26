-- To load PGroonga
SELECT pgroonga_command('status')::json->0->0;

CREATE TABLE memos (
  content text
);

INSERT INTO memos VALUES ('Groonga is fast!');

SET pgroonga.writable = false;

CREATE INDEX pgrn_index ON memos USING PGroonga (content);

SET pgroonga.writable = true;

CREATE INDEX pgrn_index ON memos USING PGroonga (content);

SELECT pgroonga_command('select',
			ARRAY[
			  'table', pgroonga_table_name('pgrn_index'),
			  'output_columns', '_id, content'
			])::jsonb->>1;

DROP TABLE memos;
