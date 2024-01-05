CREATE TABLE tags (
  names text[]
);

INSERT INTO tags VALUES (ARRAY['PostgreSQL', 'ポスグレ']);
INSERT INTO tags VALUES (ARRAY['Groonga', 'グルンガ']);
INSERT INTO tags VALUES (ARRAY['PGroonga', 'ピージールンガ']);

CREATE INDEX pgroonga_index ON tags
  USING pgroonga (names pgroonga_text_array_term_search_ops_v2)
  WITH (normalizer = 'NormalizerNFKC150("unify_kana", true)');

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT names
  FROM tags
 WHERE names &=~ pgroonga_condition('ぽすぐれ OR ぐるんが', index_name => 'pgroonga_index')
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g"

SELECT names
  FROM tags
 WHERE names &=~ pgroonga_condition('ぽすぐれ OR ぐるんが', index_name => 'pgroonga_index');
\pset format aligned

DROP TABLE tags;
