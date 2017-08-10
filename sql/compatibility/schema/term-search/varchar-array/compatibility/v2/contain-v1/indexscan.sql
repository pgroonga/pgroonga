CREATE TABLE memos (
  title text,
  tags varchar(1023)[]
);

INSERT INTO memos VALUES ('PostgreSQL', ARRAY['PostgreSQL']);
INSERT INTO memos VALUES ('Groonga', ARRAY['Groonga']);
INSERT INTO memos VALUES ('PGroonga', ARRAY['PostgreSQL', 'Groonga']);

CREATE INDEX pgroonga_memos_index ON memos
  USING pgroonga (tags pgroonga.varchar_array_term_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT title, tags
  FROM memos
 WHERE tags %% 'Groonga';

SELECT title, tags
  FROM memos
 WHERE tags %% 'Groonga';

DROP TABLE memos;
