CREATE TABLE memos (
  id integer,
  record jsonb
);

INSERT INTO memos VALUES
  (1, '{"title": "PostgreSQL", "content": "PostgreSQL is a RDBMS."}');
INSERT INTO memos VALUES
  (2, '{"title": "Groonga", "content": "Groonga is fast full text search engine."}');
INSERT INTO memos VALUES
  (3, '{"title": "PGroonga", "content": "PGroonga is a PostgreSQL extension that uses Groonga."}');

CREATE INDEX grnindex ON memos
 USING pgroonga (record pgroonga_jsonb_full_text_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT id, record
  FROM memos
 WHERE record &@ 'groonga';

SELECT id, record
  FROM memos
 WHERE record &@ 'groonga';

DROP TABLE memos;
