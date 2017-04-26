CREATE TABLE memos (
  id integer,
  content text
);

CREATE INDEX pgrn_index ON memos
 USING pgroonga (content)
  WITH (plugins = 'query_expanders/tsv');

SELECT pgroonga.command('object_exist QueryExpanderTSV')::json->>1;

DROP TABLE memos;
