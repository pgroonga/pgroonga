CREATE TABLE memos (
  content text
);
CREATE INDEX pgrn_index ON memos
 USING pgroonga (content)
  WITH (query_allow_column = true);

SELECT pgroonga_query_extract_keywords('Groonga content:@PostgreSQL',
                                       index_name => 'pgrn_index');

DROP TABLE memos;
