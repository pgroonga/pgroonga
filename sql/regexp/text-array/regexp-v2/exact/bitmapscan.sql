CREATE TABLE memos (
  id integer,
  content text[]
);

INSERT INTO memos
     VALUES (1, ARRAY['PostgreSQL',
                      'Groonga',
                      'PGroonga']);

INSERT INTO memos
     VALUES (2, ARRAY['MySQL',
                      'Mroonga']);

CREATE INDEX pgrn_content_index
    ON memos
 USING pgroonga (content pgroonga_text_array_regexp_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT *
  FROM memos
 WHERE content &~ '\Agroonga\z';

SELECT *
  FROM memos
 WHERE content &~ '\Agroonga\z';

DROP TABLE memos;
