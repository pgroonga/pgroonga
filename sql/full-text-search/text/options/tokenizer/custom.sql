CREATE TABLE memos (
  id integer,
  tags text
);

INSERT INTO memos VALUES (1, 'PostgreSQL94 RDBMS');
INSERT INTO memos VALUES (2, 'PostgreSQL Groonga');
INSERT INTO memos VALUES (3, 'Groonga PGroonga Mroonga');

CREATE INDEX grnindex ON memos
 USING pgroonga (tags)
  WITH (tokenizer = 'TokenDelimit');

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT id, tags
  FROM memos
 WHERE tags %% 'PostgreSQL';

SELECT id, tags
  FROM memos
 WHERE tags %% 'PostgreSQL94';

DROP TABLE memos;
