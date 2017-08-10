CREATE TABLE memos (
  id integer,
  content varchar(256)
);

INSERT INTO memos VALUES (1, 'PostgreSQL is a RDBMS');
INSERT INTO memos VALUES (2, 'Groonga is fast full text search engine');
INSERT INTO memos VALUES (3, 'PGroonga is a PostgreSQL extension that uses Groonga');

CREATE INDEX grnindex ON memos
  USING pgroonga (content pgroonga.varchar_regexp_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT id, content
  FROM memos
 WHERE content @~ 'groonga\z';

SELECT id, content
  FROM memos
 WHERE content @~ 'groonga\z';

DROP TABLE memos;
