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

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SELECT id, record
  FROM memos
 WHERE record &@ 'groonga';

DROP TABLE memos;
