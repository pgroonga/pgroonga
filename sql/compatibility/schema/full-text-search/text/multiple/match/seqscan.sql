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

CREATE INDEX grnindex ON memos
  USING pgroonga (title pgroonga.text_full_text_search_ops_v2,
                  content pgroonga.text_full_text_search_ops_v2);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SELECT id, title, content
  FROM memos
 WHERE title %% 'Groonga';

SELECT id, title, content
  FROM memos
 WHERE content %% 'Groonga';

DROP TABLE memos;
