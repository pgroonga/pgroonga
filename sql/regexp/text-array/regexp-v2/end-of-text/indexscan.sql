CREATE TABLE memos (
  id integer,
  content text[]
);

INSERT INTO memos
     VALUES (1, ARRAY['PostgreSQL is a RDBMS',
                      'Groonga is fast full text search engine',
                      'PGroonga is a PostgreSQL extension that uses Groonga']);

INSERT INTO memos
     VALUES (2, ARRAY['MySQL is a RDBMS',
                      'Mroonga is a MySQL storage engine that uses Groonga']);

CREATE INDEX pgrn_content_index
    ON memos
 USING pgroonga (content pgroonga_text_array_regexp_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT *
  FROM memos
 WHERE content &~ 'engine\z';

SELECT *
  FROM memos
 WHERE content &~ 'engine\z';

DROP TABLE memos;
