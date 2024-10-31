CREATE TABLE memos (
  id integer,
  user_name text,
  content text[]
);

CREATE USER alice NOLOGIN;
GRANT ALL ON TABLE memos TO alice;

INSERT INTO memos VALUES
  (1, 'nonexistent', ARRAY['PostgreSQL is a RDBMS.',
                           'MySQL is a RDBMS.',
                           'MariaDB is a RDBMS.']);
INSERT INTO memos VALUES
  (2, 'alice', ARRAY['Groonga is fast full text search engine.',
                     'PGroonga is a PostgreSQL extension that uses Groonga.',
                     'Mroonga is a MySQL storage engine that uses Groonga.']);

ALTER TABLE memos ENABLE ROW LEVEL SECURITY;
CREATE POLICY memos_myself ON memos USING (user_name = current_user);

CREATE INDEX pgrn_index ON memos
 USING pgroonga (content pgroonga_text_array_regexp_ops_v2);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SET SESSION AUTHORIZATION alice;
\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT id, content
  FROM memos
 WHERE content &~ 'pgroonga'
\g |sed -r -e "s/\(CURRENT_USER\)::text/CURRENT_USER/g"
\pset format aligned

SELECT id, content
  FROM memos
 WHERE content &~ 'pgroonga';
RESET SESSION AUTHORIZATION;

DROP TABLE memos;

DROP USER alice;
