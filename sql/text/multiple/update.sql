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

UPDATE memos
   SET title = 'Mroonga',
       content = 'is a MySQL plugin that uses Groonga.'
 WHERE id = 3;

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT id, title, content
  FROM memos
 WHERE content %% 'Groonga';

SELECT id, title, content
  FROM memos
 WHERE title %% 'Mroonga';

SELECT id, title, content
  FROM memos
 WHERE content %% 'MySQL';

DROP TABLE memos;
