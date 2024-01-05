CREATE TABLE tags (
  id integer,
  user_name text,
  names text[]
);

CREATE USER alice NOLOGIN;
GRANT ALL ON TABLE tags TO alice;

INSERT INTO tags VALUES (1, 'nonexistent', ARRAY['PostgreSQL', 'PG']);
INSERT INTO tags VALUES (2, 'alice', ARRAY['Groonga', 'grn']);
INSERT INTO tags VALUES (3, 'alice', ARRAY['PGroonga', 'pgrn']);
INSERT INTO tags VALUES (4, 'alice', ARRAY[]::text[]);

ALTER TABLE tags ENABLE ROW LEVEL SECURITY;
CREATE POLICY tags_myself ON tags USING (user_name = current_user);

CREATE INDEX pgrn_index ON tags
 USING pgroonga (names pgroonga_text_array_term_search_ops_v2)
  WITH (normalizers='NormalizerNFKC130("remove_symbol", true)');

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SET SESSION AUTHORIZATION alice;
\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT names
  FROM tags
 WHERE names &^ pgroonga_condition('-p_G', index_name => 'pgrn_index')
 ORDER BY id
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g" -e "s/\(CURRENT_USER\)::text/CURRENT_USER/g"
\pset format aligned

SELECT names
  FROM tags
 WHERE names &^ pgroonga_condition('-p_G', index_name => 'pgrn_index')
 ORDER BY id;
RESET SESSION AUTHORIZATION;

DROP TABLE tags;

DROP USER alice;
