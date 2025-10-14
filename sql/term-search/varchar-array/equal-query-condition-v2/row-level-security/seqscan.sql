CREATE TABLE tags (
  id integer,
  user_name text,
  names varchar[]
);

CREATE USER alice NOLOGIN;
GRANT ALL ON TABLE tags TO alice;

INSERT INTO tags VALUES (1, 'alice', ARRAY['PostgreSQL', 'ポスグレ']);
INSERT INTO tags VALUES (2, 'alice', ARRAY['Groonga', 'グルンガ']);
INSERT INTO tags VALUES (3, 'alice', ARRAY['PGroonga', 'ピージールンガ']);
INSERT INTO tags VALUES (4, 'nonexistent', ARRAY['Mroonga', 'ムルンガ']);
INSERT INTO tags VALUES (5, 'nonexistent', ARRAY['Groonga', 'ぐるんが']);

CREATE INDEX pgroonga_index ON tags
  USING pgroonga (names pgroonga_varchar_array_term_search_ops_v2)
  WITH (normalizer = 'NormalizerNFKC150("unify_kana", true)');

ALTER TABLE tags ENABLE ROW LEVEL SECURITY;
CREATE POLICY tags_myself ON tags USING (user_name = current_user);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SET SESSION AUTHORIZATION alice;
\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT names
  FROM tags
 WHERE names &=~ pgroonga_condition('ぽすぐれ OR ぐるんが', index_name => 'pgroonga_index')
 ORDER BY id
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g" -e "s/\(CURRENT_USER\)::text/CURRENT_USER/g"

SELECT names
  FROM tags
 WHERE names &=~ pgroonga_condition('ぽすぐれ OR ぐるんが', index_name => 'pgroonga_index')
 ORDER BY id;
\pset format aligned
RESET SESSION AUTHORIZATION;

DROP TABLE tags;

DROP USER alice;
