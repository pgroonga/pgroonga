CREATE TABLE memos (
  id integer,
  content varchar(256)
);

INSERT INTO memos VALUES (1, 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES (3, 'PGroonga is a PostgreSQL extension that uses Groonga.');

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SELECT id, content
  FROM memos
 WHERE content &~| ARRAY['\Agroonga', '\Apostgresql']::varchar[];

DROP TABLE memos;
