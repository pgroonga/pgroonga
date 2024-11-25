CREATE TABLE memos (
  content text[]
);

INSERT INTO memos
     VALUES (ARRAY['PostgreSQL is a RDBMS',
                   'Groonga is fast full text search engine',
                   'PGroonga is a PostgreSQL extension that uses Groonga']);

INSERT INTO memos
     VALUES (ARRAY['MySQL is a RDBMS',
                   'Mroonga is a MySQL storage engine that uses Groonga']);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

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
