CREATE TABLE memos (
  id integer,
  user_name text,
  tags varchar(256)[]
);

CREATE USER alice NOLOGIN;
GRANT ALL ON TABLE memos TO alice;

INSERT INTO memos VALUES (1, 'nonexistent', ARRAY['PostgreSQL']);
INSERT INTO memos VALUES (2, 'alice', ARRAY['Groonga']);
INSERT INTO memos VALUES (3, 'alice', ARRAY['PGroonga', 'PostgreSQL', 'Groonga']);

ALTER TABLE memos ENABLE ROW LEVEL SECURITY;
CREATE POLICY memos_myself ON memos USING (user_name = current_user);

CREATE INDEX pgrn_index ON memos
 USING pgroonga (tags pgroonga_varchar_array_term_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

SET SESSION AUTHORIZATION alice;
EXPLAIN (COSTS OFF)
SELECT tags
  FROM memos
 WHERE tags &> 'Groonga'
 ORDER BY id;

SELECT tags
  FROM memos
 WHERE tags &> 'Groonga'
 ORDER BY id;
RESET SESSION AUTHORIZATION;

DROP TABLE memos;

DROP USER alice;
