CREATE TABLE tags (
  name text
);

INSERT INTO tags VALUES ('PostgreSQL');
INSERT INTO tags VALUES ('Groonga');
INSERT INTO tags VALUES ('PGroonga');
INSERT INTO tags VALUES ('pglogical');

CREATE INDEX pgrn_index ON tags
  USING pgroonga (name pgroonga.text_term_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT name
  FROM tags
 WHERE name &^ 'pG';

DROP TABLE tags;
