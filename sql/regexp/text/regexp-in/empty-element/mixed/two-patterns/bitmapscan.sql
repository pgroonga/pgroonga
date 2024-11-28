CREATE TABLE memos (
  content text
);

INSERT INTO memos VALUES ('PostgreSQL is a RDBMS');
INSERT INTO memos VALUES ('Groonga is fast full text search engine');
INSERT INTO memos VALUES ('PGroonga is a PostgreSQL extension that uses Groonga');

CREATE INDEX pgrn_content_index ON memos
  USING pgroonga (content pgroonga_text_regexp_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT *
  FROM memos
 WHERE content &~| ARRAY['', 'rdbms\z', '\Agroonga'];

SELECT *
  FROM memos
 WHERE content &~| ARRAY['', 'rdbms\z', '\Agroonga'];

DROP TABLE memos;
