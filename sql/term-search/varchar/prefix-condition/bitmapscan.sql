CREATE TABLE tags (
  name varchar
);

INSERT INTO tags VALUES ('PostgreSQL');
INSERT INTO tags VALUES ('Groonga');
INSERT INTO tags VALUES ('PGroonga');
INSERT INTO tags VALUES ('pglogical');

CREATE INDEX pgrn_index ON tags
  USING pgroonga (name pgroonga_varchar_term_search_ops_v2)
  WITH (normalizers='NormalizerNFKC130("remove_symbol", true)');

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT name
  FROM tags
 WHERE name &^ ('-p_G', NULL, 'pgrn_index')::pgroonga_full_text_search_condition;
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g"
\pset format aligned

SELECT name
  FROM tags
 WHERE name &^ ('-p_G', NULL, 'pgrn_index')::pgroonga_full_text_search_condition;

DROP TABLE tags;
