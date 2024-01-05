CREATE TABLE tags (
  names varchar[]
);

INSERT INTO tags VALUES (ARRAY['PostgreSQL', 'PG']);
INSERT INTO tags VALUES (ARRAY['Groonga', 'grn']);
INSERT INTO tags VALUES (ARRAY['PGroonga', 'pgrn']);
INSERT INTO tags VALUES (ARRAY[]::varchar[]);

CREATE INDEX pgrn_index ON tags
  USING pgroonga (names);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT names
  FROM tags
 WHERE names &^ 'pG';

SELECT names
  FROM tags
 WHERE names &^ 'pG';

DROP TABLE tags;
