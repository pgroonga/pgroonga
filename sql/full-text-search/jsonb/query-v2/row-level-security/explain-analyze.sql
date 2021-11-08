CREATE TABLE memos (
  record jsonb
);

INSERT INTO memos VALUES
  ('{"title": "PostgreSQL", "content": "PostgreSQL is a RDBMS."}');

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

EXPLAIN ANALYZE
SELECT record
  FROM memos
 WHERE record &@~ 'rdbms OR pgroonga'
\g |sed -r -e "s/\(CURRENT_USER\)::text/CURRENT_USER/g"
\pset format aligned

DROP TABLE memos;
