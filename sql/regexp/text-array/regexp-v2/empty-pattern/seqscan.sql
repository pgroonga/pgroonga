CREATE TABLE memos (
  contents text[]
);

INSERT INTO memos
     VALUES (ARRAY['PostgreSQL is a RDBMS',
                   'Groonga is fast full text search engine',
                   'PGroonga is a PostgreSQL extension that uses Groonga']);

EXPLAIN (COSTS OFF)
SELECT *
  FROM memos
 WHERE contents &~ '';

SELECT *
  FROM memos
 WHERE contents &~ '';

DROP TABLE memos;
