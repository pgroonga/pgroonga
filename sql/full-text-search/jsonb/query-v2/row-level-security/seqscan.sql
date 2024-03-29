CREATE TABLE memos (
  id integer,
  user_name text,
  record jsonb
);

CREATE USER alice NOLOGIN;
GRANT ALL ON TABLE memos TO alice;

INSERT INTO memos VALUES
  (1,
   'nonexistent',
   '{"title": "PostgreSQL", "content": "PostgreSQL is a RDBMS."}');
INSERT INTO memos VALUES
  (2,
   'alice',
   '{"title": "Groonga", "content": "Groonga is fast full text search engine."}');
INSERT INTO memos VALUES
  (3,
   'alice',
   '{"title": "PGroonga", "content": "PGroonga is a PostgreSQL extension that uses Groonga."}');

ALTER TABLE memos ENABLE ROW LEVEL SECURITY;
CREATE POLICY memos_myself ON memos USING (user_name = current_user);

CREATE INDEX pgrn_index ON memos
 USING pgroonga (record pgroonga_jsonb_full_text_search_ops_v2);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SET SESSION AUTHORIZATION alice;
\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT id, record
  FROM memos
 WHERE record &@~ 'rdbms OR pgroonga'
\g |sed -r -e "s/\(CURRENT_USER\)::text/CURRENT_USER/g"
\pset format aligned

SELECT id, record
  FROM memos
 WHERE record &@~ 'rdbms OR pgroonga';
RESET SESSION AUTHORIZATION;

DROP TABLE memos;

DROP USER alice;
