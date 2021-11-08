CREATE TABLE memos (
  contents text[]
);

INSERT INTO memos VALUES
  (ARRAY['PostgreSQL is an OSS RDBMS',
         'PostgreSQL has partial full-text search support']);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

EXPLAIN ANALYZE
SELECT contents
  FROM memos
 WHERE contents &@
       ('PostgreSQL',
        NULL,
        NULL,
        'pgroonga_memos_index')::pgroonga_full_text_search_condition_with_scorers
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g" -e "s/\(CURRENT_USER\)::text/CURRENT_USER/g"
\pset format aligned

DROP TABLE memos;
