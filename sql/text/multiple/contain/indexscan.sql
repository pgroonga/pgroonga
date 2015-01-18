CREATE TABLE memos (
  id integer,
  title text,
  content text
);

INSERT INTO memos
     VALUES (1, 'PostgreSQL', 'is a RDBMS.');
INSERT INTO memos
     VALUES (2, 'Groonga', 'is fast full text search engine.');
INSERT INTO memos
     VALUES (3, 'PGroonga', 'is a PostgreSQL extension that uses Groonga.');

CREATE INDEX grnindex ON memos USING pgroonga (title, content);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT id, title, content
  FROM memos
 WHERE title %% 'Groonga' OR content %% 'Groonga';

DROP TABLE memos;
