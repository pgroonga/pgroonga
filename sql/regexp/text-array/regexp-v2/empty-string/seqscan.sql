CREATE TABLE memos (
  content text[]
);

INSERT INTO memos
     VALUES (ARRAY['PostgreSQL is a RDBMS',
                   'Groonga is fast full text search engine',
                   'PGroonga is a PostgreSQL extension that uses Groonga']);

EXPLAIN (COSTS OFF)
SELECT *
  FROM memos
 WHERE content &~ '';

SELECT *
  FROM memos
 WHERE content &~ '';

DROP TABLE memos;
