CREATE TABLE tags (
  names varchar[]
);

INSERT INTO tags VALUES (ARRAY['PostgreSQL', 'PG']);
INSERT INTO tags VALUES (ARRAY['Groonga', 'grn']);
INSERT INTO tags VALUES (ARRAY['PGroonga', 'pgrn']);
INSERT INTO tags VALUES (ARRAY[]::varchar[]);

CREATE INDEX pgrn_index ON tags
  USING pgroonga (names)
  WITH (normalizers='NormalizerNFKC130("remove_symbol", true)');

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT names
  FROM tags
 WHERE names &^ pgroonga_condition('-p_G', index_name => 'pgrn_index')
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g"
\pset format aligned

SELECT names
  FROM tags
 WHERE names &^ pgroonga_condition('-p_G', index_name => 'pgrn_index');

DROP TABLE tags;
