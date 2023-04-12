CREATE TABLE tags (
  names varchar[]
);

INSERT INTO tags VALUES (ARRAY['PostgreSQL', 'PG']);
INSERT INTO tags VALUES (ARRAY['Groonga', 'grn', 'groonga']);
INSERT INTO tags VALUES (ARRAY['PGroonga', 'pgrn', 'sql']);

CREATE INDEX pgroonga_index ON tags
  USING pgroonga (names pgroonga_varchar_array_term_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT names
  FROM tags
 WHERE names &=~ 'grn OR sql';

SELECT names
  FROM tags
 WHERE names &=~ 'grn OR sql';

DROP TABLE tags;
