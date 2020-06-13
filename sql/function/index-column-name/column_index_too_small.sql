CREATE TABLE memos (
  id integer,
  title text,
  content text,
  tag text
);

CREATE INDEX pgroonga_index
          ON memos
       USING pgroonga (title, content, tag);

SELECT pgroonga_index_column_name('pgroonga_index', -1);

DROP TABLE memos;
