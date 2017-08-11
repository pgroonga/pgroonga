CREATE TABLE tags (
  names text[]
);

INSERT INTO tags VALUES (ARRAY['PostgreSQL', 'PG']);
INSERT INTO tags VALUES (ARRAY['Groonga', 'grn']);
INSERT INTO tags VALUES (ARRAY['PGroonga', 'pgrn']);

CREATE INDEX pgrn_index ON tags
  USING pgroonga (names pgroonga_text_array_term_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT names
  FROM tags
 WHERE names &^| ARRAY['gro', 'pos'];

SELECT names
  FROM tags
 WHERE names &^| ARRAY['gro', 'pos'];

DROP TABLE tags;
