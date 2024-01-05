CREATE TABLE tags (
  id int,
  names varchar[]
);

INSERT INTO tags VALUES (1, ARRAY['PostgreSQL', 'PG']);
INSERT INTO tags VALUES (2, ARRAY['Groonga', 'grn']);
INSERT INTO tags VALUES (3, ARRAY['PGroonga', 'pgrn']);
INSERT INTO tags VALUES (4, ARRAY[]::varchar[]);

CREATE INDEX pgrn_index ON tags
  USING pgroonga (names);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT names
  FROM tags
 WHERE names &^ 'pG'
 ORDER BY id;

SELECT names
  FROM tags
 WHERE names &^ 'pG'
 ORDER BY id;

DROP TABLE tags;
