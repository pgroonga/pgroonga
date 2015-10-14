CREATE TABLE memos (
  id integer,
  content text
);

INSERT INTO memos VALUES (1, 'PostgreSQL is a RDBMS');
INSERT INTO memos VALUES (2, 'Groonga is fast full text search engine');
INSERT INTO memos VALUES (3, 'PGroonga is a PostgreSQL extension that uses Groonga');

CREATE INDEX grnindex ON memos USING pgroonga (content pgroonga.text_regexp_ops);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SELECT id, content
  FROM memos
 WHERE content @~ 'groonga\z';

DROP TABLE memos;
