CREATE TABLE tags (
  id int,
  name text
);

INSERT INTO tags VALUES (1, 'PostgreSQL');
INSERT INTO tags VALUES (2, 'Groonga');
INSERT INTO tags VALUES (3, 'groonga');
INSERT INTO tags VALUES (4, 'PGroonga');

CREATE INDEX pgrn_index ON tags
  USING pgroonga (name pgroonga_text_term_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT name
  FROM tags
 WHERE name &= pgroonga_condition('groonga', index_name => 'pgrn_index')
 ORDER BY id
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g"
\pset format aligned

SELECT name
  FROM tags
 WHERE name &= pgroonga_condition('groonga', index_name => 'pgrn_index')
 ORDER BY id;

DROP TABLE tags;
