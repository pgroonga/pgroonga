CREATE TABLE tags (
  id int PRIMARY KEY,
  names text[],
  readings text[]
);

INSERT INTO tags VALUES (1,
                         ARRAY['PostgreSQL', 'PG'],
                         ARRAY['ポストグレスキューエル', 'ポスグレ']);
INSERT INTO tags VALUES (2,
                         ARRAY['Groonga', 'grn'],
                         ARRAY['グルンガ', 'グルン']);
INSERT INTO tags VALUES (3,
                         ARRAY['PGroonga', 'pgrn'],
                         ARRAY['ピージールンガ', 'ピーグルン']);

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

SELECT names, pgroonga_score(tags)
  FROM tags
  WHERE names &^ 'Groon' OR
        readings &^~ 'posu';

DROP TABLE tags;
