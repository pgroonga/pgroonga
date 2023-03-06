CREATE TABLE tags (
  id integer,
  user_name text,
  name text
);

CREATE USER alice NOLOGIN;
GRANT ALL ON TABLE tags TO alice;

INSERT INTO tags VALUES (1, 'nonexistent', 'PostgreSQL');
INSERT INTO tags VALUES (2, 'alice', 'Groonga');
INSERT INTO tags VALUES (3, 'alice', 'PGroonga');
INSERT INTO tags VALUES (4, 'alice', 'pglogical');

ALTER TABLE tags ENABLE ROW LEVEL SECURITY;
CREATE POLICY tags_myself ON tags USING (user_name = current_user);

CREATE INDEX pgrn_index ON tags
 USING pgroonga (name pgroonga_text_term_search_ops_v2)
  WITH (normalizers='NormalizerNFKC130("remove_symbol", true)');

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

SET SESSION AUTHORIZATION alice;
\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT name
  FROM tags
 WHERE name &^ ('-p_G', NULL, 'pgrn_index')::pgroonga_full_text_search_condition
 ORDER BY id
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g" -e "s/\(CURRENT_USER\)::text/CURRENT_USER/g"
\pset format aligned

SELECT name
  FROM tags
 WHERE name &^ ('-p_G', NULL, 'pgrn_index')::pgroonga_full_text_search_condition
 ORDER BY id;
RESET SESSION AUTHORIZATION;

DROP TABLE tags;

DROP USER alice;
