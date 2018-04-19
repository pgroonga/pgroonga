CREATE TABLE memos (
  id integer,
  title text,
  content text
);

INSERT INTO memos VALUES (1, 'PostgreSQL', 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'Groonga', 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES (3, 'PGroonga', 'PGroonga is a PostgreSQL extension that uses Groonga.');

CREATE INDEX pgrn_index ON memos
 USING pgroonga ((ARRAY[title, content])
                 pgroonga_text_array_full_text_search_ops_v2);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT id, title, content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE ARRAY[title, content] &@
       ('PostgreSQL',
        NULL,
        NULL,
        'pgrn_index')::pgroonga_full_text_search_condition_with_scorers;

SELECT id, title, content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE ARRAY[title, content] &@
       ('PostgreSQL',
        NULL,
        NULL,
        'pgrn_index')::pgroonga_full_text_search_condition_with_scorers;

DROP TABLE memos;
