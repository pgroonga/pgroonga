CREATE TABLE memos (
  id integer,
  title text,
  content text,
  tag text
);

CREATE INDEX pgroonga_index
          ON memos
       USING pgroonga (title, content, tag);

SELECT regexp_replace(pgroonga_index_column_name('pgroonga_index', 'tag'),
                      'pgroonga_index'::regclass::oid::text,
                      '${OID}');

DROP TABLE memos;

