CREATE TABLE tags (
  id integer,
  user_name text,
  names varchar[]
);

CREATE USER alice NOLOGIN;
GRANT ALL ON TABLE tags TO alice;

INSERT INTO tags VALUES (1, 'nonexistent', ARRAY['PostgreSQL', 'PG']);
INSERT INTO tags VALUES (2, 'alice', ARRAY['Groonga', 'grn']);
INSERT INTO tags VALUES (3, 'alice', ARRAY['PGroonga', 'pgrn']);
INSERT INTO tags VALUES (4, 'alice', ARRAY[]::varchar[]);

ALTER TABLE tags ENABLE ROW LEVEL SECURITY;
CREATE POLICY tags_myself ON tags USING (user_name = current_user);

CREATE INDEX pgrn_index ON tags
 USING pgroonga (names);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

SET SESSION AUTHORIZATION alice;
\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT names
  FROM tags
 WHERE names &^ 'pG'
 ORDER BY id
\g |sed -r -e "s/\(CURRENT_USER\)::text/CURRENT_USER/g"
\pset format aligned

SELECT names
  FROM tags
 WHERE names &^ 'pG'
 ORDER BY id;
RESET SESSION AUTHORIZATION;

DROP TABLE tags;

DROP USER alice;
