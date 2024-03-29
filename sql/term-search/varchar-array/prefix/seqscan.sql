CREATE TABLE tags (
  names varchar[]
);

INSERT INTO tags VALUES (ARRAY['PostgreSQL', 'PG']);
INSERT INTO tags VALUES (ARRAY['Groonga', 'grn']);
INSERT INTO tags VALUES (ARRAY['PGroonga', 'pgrn']);
INSERT INTO tags VALUES (ARRAY[]::varchar[]);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SELECT names
  FROM tags
 WHERE names &^ 'pG';

DROP TABLE tags;
