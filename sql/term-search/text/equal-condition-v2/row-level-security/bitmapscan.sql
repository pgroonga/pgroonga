CREATE TABLE tags (
  id integer,
  user_name text,
  name text
);

CREATE USER alice NOLOGIN;
GRANT ALL ON TABLE tags TO alice;

INSERT INTO tags VALUES (1, 'nonexistent', 'PostgreSQL');
INSERT INTO tags VALUES (2, 'nonexistent', 'GROONGA');
INSERT INTO tags VALUES (3, 'alice', 'Groonga');
INSERT INTO tags VALUES (4, 'alice', 'groonga');
INSERT INTO tags VALUES (5, 'alice', 'PGroonga');

ALTER TABLE tags ENABLE ROW LEVEL SECURITY;
CREATE POLICY tags_myself ON tags USING (user_name = current_user);

CREATE INDEX pgrn_index ON tags
 USING pgroonga (name pgroonga_text_term_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

SET SESSION AUTHORIZATION alice;
\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT name
  FROM tags
 WHERE name &= pgroonga_condition('groonga', index_name => 'pgrn_index')
 ORDER BY id
\g |sed -r -e "s/\(CURRENT_USER\)::text/CURRENT_USER/g"
\pset format aligned

SELECT name
  FROM tags
 WHERE name &= pgroonga_condition('groonga', index_name => 'pgrn_index')
 ORDER BY id;
RESET SESSION AUTHORIZATION;

DROP TABLE tags;

DROP USER alice;
