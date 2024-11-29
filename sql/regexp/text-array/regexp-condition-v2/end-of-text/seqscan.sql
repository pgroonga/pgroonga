CREATE TABLE memos (
  contents text[]
);

INSERT INTO memos
     VALUES (ARRAY['PostgreSQL is a RDBMS',
                   'Groonga is fast full text search engine!!!',
                   'PGroonga is a PostgreSQL extension that uses Groonga']);

INSERT INTO memos
     VALUES (ARRAY['MySQL is a RDBMS',
                   'Mroonga is a MySQL storage engine that uses Groonga']);

CREATE INDEX pgrn_contents_index
    ON memos
 USING pgroonga (contents pgroonga_text_array_regexp_ops_v2)
  WITH (normalizers='NormalizerNFKC150("remove_symbol", true)');

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT *
  FROM memos
 WHERE contents &~ pgroonga_condition('engine\z',
                                      index_name => 'pgrn_contents_index');

SELECT *
  FROM memos
 WHERE contents &~ pgroonga_condition('engine\z',
                                      index_name => 'pgrn_contents_index');

DROP TABLE memos;
