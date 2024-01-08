CREATE TABLE tags (
  names varchar[]
);

INSERT INTO tags VALUES (ARRAY['PG', 'PostgreSQL']);
INSERT INTO tags VALUES (ARRAY['grn', 'Groonga']);
INSERT INTO tags VALUES (ARRAY['pgrn', 'PGroonga']);
INSERT INTO tags VALUES (ARRAY[]::varchar[]);

CREATE INDEX pgrn_index ON tags
  USING pgroonga (names)
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
