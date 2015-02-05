DROP TABLE IF EXISTS memos;
CREATE TABLE memos (
  id integer,
  title text
);

INSERT INTO memos VALUES (2, 'Groonga');
INSERT INTO memos VALUES (3, 'PGroonga');
INSERT INTO memos VALUES (1, 'PostgreSQL');

CREATE INDEX grnindex ON memos USING pgroonga (id);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = on;

SELECT id, title
  FROM memos
 ORDER BY id DESC LIMIT 2;

DROP TABLE memos;

DROP TABLE IF EXISTS memos;
CREATE TABLE memos (
  id integer,
  content text
);

INSERT INTO memos VALUES (1, 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'Groonga is fast full text search engine.');

CREATE INDEX grnindex ON memos USING pgroonga (content);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = on;

SELECT id, content
  FROM memos
 WHERE content %% 'is';

SELECT id, content
  FROM memos
 WHERE content %% 'text';

INSERT INTO memos VALUES (3, 'PGroonga is a PostgreSQL plugin that uses Groonga.');

SELECT id, content
  FROM memos
 WHERE content %% 'postgresql';

UPDATE memos SET content = 'Mroonga is a MySQL plugin that uses Groonga.'
 WHERE id = 3;

SELECT id, content
  FROM memos
 WHERE content %% 'postgresql';

SELECT id, content
  FROM memos
 WHERE content %% 'mysql';

SELECT id, content
  FROM memos
 WHERE content %% 'mysql' OR content %% 'groonga';
