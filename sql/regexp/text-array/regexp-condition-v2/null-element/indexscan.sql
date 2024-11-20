CREATE TABLE memos (
  id integer,
  content text[]
);

INSERT INTO memos
     VALUES (1, ARRAY[NULL,
                      'Groonga is fast full text search engine!!!',
                      'PGroonga is a PostgreSQL extension that uses Groonga']);

INSERT INTO memos
     VALUES (2, ARRAY['MySQL is a RDBMS', NULL]);

CREATE INDEX pgrn_content_index
    ON memos
 USING pgroonga (content pgroonga_text_array_regexp_ops_v2)
  WITH (normalizers='NormalizerNFKC150("remove_symbol", true)');

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT *
  FROM memos
 WHERE content &~ pgroonga_condition('engine\z',
                                     index_name => 'pgrn_content_index');

SELECT *
  FROM memos
 WHERE content &~ pgroonga_condition('engine\z',
                                     index_name => 'pgrn_content_index');

DROP TABLE memos;
