CREATE TABLE memos (
  id integer,
  content text
);

INSERT INTO memos VALUES (1, 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES (3, 'PGroonga is a PostgreSQL extension that uses Groonga.');

CREATE INDEX grnindex ON memos USING pgroonga (content);

UPDATE memos SET content = 'Mroonga is a MySQL plugin that uses Groonga.'
 WHERE id = 3;

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT id, content
  FROM memos
 WHERE content %% 'PostgreSQL';

SELECT id, content
  FROM memos
 WHERE content %% 'MySQL';

DROP TABLE memos;
