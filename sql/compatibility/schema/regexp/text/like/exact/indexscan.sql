CREATE TABLE memos (
  id integer,
  content text
);

INSERT INTO memos VALUES (1, 'PostgreSQL');
INSERT INTO memos VALUES (2, 'Groonga');
INSERT INTO memos VALUES (3, 'PGroonga');
INSERT INTO memos VALUES (4, 'groonga');

CREATE INDEX grnindex ON memos USING pgroonga (content pgroonga.text_regexp_ops);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT id, content
  FROM memos
 WHERE content LIKE 'Groonga';

DROP TABLE memos;
