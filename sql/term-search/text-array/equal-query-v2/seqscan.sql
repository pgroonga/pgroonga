CREATE TABLE tags (
  names text[]
);

INSERT INTO tags VALUES (ARRAY['PostgreSQL', 'PG']);
INSERT INTO tags VALUES (ARRAY['Groonga', 'grn', 'groonga']);
INSERT INTO tags VALUES (ARRAY['PGroonga', 'pgrn', 'SQL']);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT names
  FROM tags
 WHERE names &=~ 'grn OR sql';

SELECT names
  FROM tags
 WHERE names &=~ 'grn OR sql';

DROP TABLE tags;
