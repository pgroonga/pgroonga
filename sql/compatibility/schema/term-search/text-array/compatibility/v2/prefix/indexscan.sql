CREATE TABLE tags (
  id int,
  names text[]
);

INSERT INTO tags VALUES (1, ARRAY['PostgreSQL', 'PG']);
INSERT INTO tags VALUES (2, ARRAY['Groonga', 'grn']);
INSERT INTO tags VALUES (3, ARRAY['PGroonga', 'pgrn']);

CREATE INDEX pgrn_index ON tags
  USING pgroonga (names pgroonga.text_array_term_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT names
  FROM tags
 WHERE names &^> 'pG'
 ORDER BY id;

SELECT names
  FROM tags
 WHERE names &^> 'pG'
 ORDER BY id;

DROP TABLE tags;
