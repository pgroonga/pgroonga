CREATE TABLE memos (
  created_date varchar(10),
  slug varchar(100),
  content text,
  PRIMARY KEY (created_date, slug)
);

INSERT INTO memos VALUES
  ('2015-11-19', 'postgresql', 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES
  ('2015-11-19', 'groonga', 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES
  ('2015-11-19', 'pgroonga', 'PGroonga is a PostgreSQL extension that uses Groonga.');

CREATE INDEX grnindex ON memos USING pgroonga (created_date, slug, content);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT created_date, slug, content, pgroonga_score(memos)
  FROM memos
 WHERE content %% 'Groonga';

DROP TABLE memos;
