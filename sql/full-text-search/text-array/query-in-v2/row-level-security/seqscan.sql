CREATE TABLE memos (
  id integer,
  user_name text,
  contents text[]
);

CREATE USER alice NOLOGIN;
GRANT ALL ON TABLE memos TO alice;

INSERT INTO memos VALUES
  (1,
   'nonexistent',
   ARRAY['PostgreSQL is an OSS RDBMS',
         'PostgreSQL has partial full-text search support']);
INSERT INTO memos VALUES
  (2,
   'alice',
    ARRAY['Groonga is an OSS full-text search engine',
          'Groonga has full full-text search support']);
INSERT INTO memos VALUES
  (3,
   'alice',
   ARRAY['PGroonga is an OSS PostgreSQL extension',
         'PGroonga adds full full-text search support based on Groonga to PostgreSQL']);

ALTER TABLE memos ENABLE ROW LEVEL SECURITY;
CREATE POLICY memos_myself ON memos USING (user_name = current_user);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SET SESSION AUTHORIZATION alice;
\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT id, contents
  FROM memos
 WHERE contents &@~| Array['rdbms', 'groonga engine']
\g |sed -r -e "s/\(CURRENT_USER\)::text/CURRENT_USER/g"
\pset format aligned

SELECT id, contents
  FROM memos
 WHERE contents &@~| Array['rdbms', 'groonga engine'];
RESET SESSION AUTHORIZATION;

DROP TABLE memos;

DROP USER alice;
