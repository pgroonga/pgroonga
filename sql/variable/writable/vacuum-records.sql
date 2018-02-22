CREATE TABLE memos (
  content text
);

CREATE INDEX pgrn_index ON memos USING PGroonga (content);

INSERT INTO memos VALUES ('Groonga is fast!');
DELETE FROM memos;

SET pgroonga.writable = false;

VACUUM memos;
SELECT pgroonga_command('select',
			ARRAY[
			  'table', pgroonga_table_name('pgrn_index'),
			  'output_columns', '_id, content'
			])::jsonb->>1;

SET pgroonga.writable = true;

INSERT INTO memos VALUES ('PGroonga is fast!');
DELETE FROM memos;

VACUUM memos;
SELECT pgroonga_command('select',
			ARRAY[
			  'table', pgroonga_table_name('pgrn_index'),
			  'output_columns', '_id, content'
			])::jsonb->>1;

DROP TABLE memos;
