CREATE TABLE tags (
  names text[]
);

INSERT INTO tags VALUES (ARRAY['PostgreSQL', 'PG']);
INSERT INTO tags VALUES (ARRAY['Groonga', 'grn']);
INSERT INTO tags VALUES (ARRAY['PGroonga', 'pgrn']);

CREATE INDEX pgrn_index ON tags
  USING pgroonga (names pgroonga.text_array_term_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT names
  FROM tags
 WHERE names &^> 'pG';

DROP TABLE tags;
