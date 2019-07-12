CREATE TABLE memos (
  id integer,
  content text
);

INSERT INTO memos VALUES (1, 'PostgreSQL');
INSERT INTO memos VALUES (2, 'Groonga');
INSERT INTO memos VALUES (3, 'PGroonga');

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SELECT id, content
  FROM memos
 WHERE content &~| ARRAY['\Agroonga\z', '\Apostgresql\z'];

DROP TABLE memos;
