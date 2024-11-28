CREATE TABLE memos (
  content varchar(256)
);

INSERT INTO memos VALUES ('PostgreSQL is a RDBMS');
INSERT INTO memos VALUES ('Groonga is fast full text search engine');
INSERT INTO memos VALUES ('PGroonga is a PostgreSQL extension that uses Groonga');

CREATE INDEX pgrn_content_index ON memos
  USING pgroonga (content pgroonga_varchar_regexp_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT *
  FROM memos
 WHERE content &~| ARRAY['', 'rdbms\z']::varchar[];

SELECT *
  FROM memos
 WHERE content &~| ARRAY['', 'rdbms\z']::varchar[];

DROP TABLE memos;
