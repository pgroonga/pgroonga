CREATE TABLE tags (
  names varchar(1023)[]
);

INSERT INTO tags VALUES (ARRAY['PostgreSQL', 'PG']);
INSERT INTO tags VALUES (ARRAY['Groonga', 'grn', 'groonga']);
INSERT INTO tags VALUES (ARRAY['PGroonga', 'pgrn', 'groonga']);

CREATE INDEX pgroonga_index ON tags
  USING pgroonga (names pgroonga_varchar_array_term_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT names
  FROM tags
 WHERE names @> ARRAY['grn', 'groonga']::varchar[];

SELECT names
  FROM tags
 WHERE names @> ARRAY['grn', 'groonga']::varchar[];

DROP TABLE tags;
