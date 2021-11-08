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
 WHERE contents &@ 'Groonga'
\g |sed -r -e "s/\(CURRENT_USER\)::text/CURRENT_USER/g"
\pset format aligned

DROP TABLE memos;
