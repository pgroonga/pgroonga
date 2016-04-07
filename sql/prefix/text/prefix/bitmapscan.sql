CREATE TABLE tags (
  name text
);

INSERT INTO tags VALUES ('PostgreSQL');
INSERT INTO tags VALUES ('Groonga');
INSERT INTO tags VALUES ('PGroonga');
INSERT INTO tags VALUES ('pglogical');

CREATE INDEX pgrn_index ON tags
  USING pgroonga (name pgroonga.prefix_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

SELECT name
  FROM tags
 WHERE name &^ 'pG';

DROP TABLE tags;
