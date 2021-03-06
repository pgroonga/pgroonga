CREATE TABLE memos (
  id integer,
  tag text
);

INSERT INTO memos VALUES (1, 'PostgreSQL');
INSERT INTO memos VALUES (2, 'PostgreSQL Groonga');
INSERT INTO memos VALUES (3, 'Groonga');

CREATE INDEX grnindex ON memos
 USING pgroonga (tag)
  WITH (tokenizer = 'none');

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT id, tag
  FROM memos
 WHERE tag %% 'PostgreSQL';

SELECT id, tag
  FROM memos
 WHERE tag %% 'PostgreSQL Groonga';

DROP TABLE memos;
