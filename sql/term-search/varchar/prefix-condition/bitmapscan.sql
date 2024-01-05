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
 WHERE name &^ pgroonga_condition('-p_G', index_name => 'pgrn_index')
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g"
\pset format aligned

SELECT name
  FROM tags
 WHERE name &^ pgroonga_condition('-p_G', index_name => 'pgrn_index');

DROP TABLE tags;
