CREATE TABLE memos (
  content text[]
);

INSERT INTO memos
     VALUES (ARRAY['PostgreSQL is a RDBMS',
                   'Groonga is fast full text search engine',
                   'PGroonga is a PostgreSQL extension that uses Groonga']);

INSERT INTO memos
     VALUES (ARRAY['PostgreSQL',
                   'MySQL']);

INSERT INTO memos
     VALUES (ARRAY['MySQL is a RDBMS',
                   'Mroonga is a MySQL storage engine that uses Groonga']);

CREATE INDEX pgrn_content_index
    ON memos
 USING pgroonga (content pgroonga_text_array_regexp_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT *
  FROM memos
 WHERE content &~ pgroonga_condition('	 ',
                                     index_name => 'pgrn_content_index');

SELECT *
  FROM memos
 WHERE content &~ pgroonga_condition('	 ',
                                     index_name => 'pgrn_content_index');

DROP TABLE memos;
