CREATE TABLE memos (
  id integer,
  user_name text,
  content text
);

CREATE USER alice NOLOGIN;
GRANT ALL ON TABLE memos TO alice;

INSERT INTO memos VALUES
  (1, 'nonexistent', 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES
  (2, 'nonexistent', 'Rroonga is the Ruby bindings of Groonga.');
INSERT INTO memos VALUES
  (3, 'alice', 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES
  (4, 'alice', 'PGroonga is a PostgreSQL extension that uses Groonga.');

ALTER TABLE memos ENABLE ROW LEVEL SECURITY;
CREATE POLICY memos_myself ON memos USING (user_name = current_user);

CREATE INDEX pgrn_index ON memos
 USING pgroonga (content pgroonga_text_full_text_search_ops_v2);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SET SESSION AUTHORIZATION alice;
\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT id, content
  FROM memos
 WHERE content &@
       pgroonga_condition('Groonga', index_name => 'pgrn_index')
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g" -e "s/\(CURRENT_USER\)::text/CURRENT_USER/g"
\pset format aligned

SELECT id, content
  FROM memos
 WHERE content &@
       pgroonga_condition('Groonga', index_name => 'pgrn_index');
RESET SESSION AUTHORIZATION;

DROP TABLE memos;

DROP USER alice;
