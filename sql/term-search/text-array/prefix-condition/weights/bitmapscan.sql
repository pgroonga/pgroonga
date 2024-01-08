CREATE TABLE tags (
  names text[]
);

INSERT INTO tags VALUES (ARRAY['PG', 'PostgreSQL']);
INSERT INTO tags VALUES (ARRAY['grn', 'Groonga']);
INSERT INTO tags VALUES (ARRAY['pgrn', 'PGroonga']);
INSERT INTO tags VALUES (ARRAY[]::text[]);

CREATE INDEX pgrn_index ON tags
  USING pgroonga (names pgroonga_text_array_term_search_ops_v2)
  WITH (normalizers='NormalizerNFKC130("remove_symbol", true)');

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT names
  FROM tags
 WHERE names &^ pgroonga_condition('-p_G', ARRAY[0, 5], index_name => 'pgrn_index')
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g"
\pset format aligned

SELECT names
  FROM tags
 WHERE names &^ pgroonga_condition('-p_G', ARRAY[0, 5], index_name => 'pgrn_index');

DROP TABLE tags;
