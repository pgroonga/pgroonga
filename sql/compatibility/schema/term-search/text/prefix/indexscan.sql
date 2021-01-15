CREATE TABLE tags (
  id int,
  name text
);

INSERT INTO tags VALUES (1, 'PostgreSQL');
INSERT INTO tags VALUES (2, 'Groonga');
INSERT INTO tags VALUES (3, 'PGroonga');
INSERT INTO tags VALUES (4, 'pglogical');

CREATE INDEX pgrn_index ON tags
  USING pgroonga (name pgroonga.text_term_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT name
  FROM tags
 WHERE name &^ 'pG'
 ORDER BY id;

SELECT name
  FROM tags
 WHERE name &^ 'pG'
 ORDER BY id;

DROP TABLE tags;
