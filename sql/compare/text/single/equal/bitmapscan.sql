CREATE TABLE memos (
  id integer,
  title text
);

INSERT INTO memos VALUES (1, 'PostgreSQL');
INSERT INTO memos VALUES (2, 'Groonga');
INSERT INTO memos VALUES (3, 'PGroonga');

CREATE INDEX grnindex ON memos USING pgroonga (content pgroonga.text_ops);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

SELECT id, title
  FROM memos
 WHERE title = 'Groonga';

DROP TABLE memos;
