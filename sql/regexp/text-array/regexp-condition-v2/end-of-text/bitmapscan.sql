CREATE TABLE memos (
  contents text[]
);

INSERT INTO memos
     VALUES (ARRAY['PostgreSQL is a RDBMS',
                   'Groonga is fast full text search engine',
                   'PGroonga is a PostgreSQL extension that uses Groonga']);

INSERT INTO memos
     VALUES (ARRAY['MySQL is a RDBMS',
                   'Mroonga is a MySQL storage engine that uses Groonga']);

CREATE INDEX pgrn_contents_index
    ON memos
 USING pgroonga (contents pgroonga_text_array_regexp_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT *
  FROM memos
 WHERE contents &~ pgroonga_condition('engine\z');

SELECT *
  FROM memos
 WHERE contents &~ pgroonga_condition('engine\z');

DROP TABLE memos;
