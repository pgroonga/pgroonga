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

CREATE INDEX pgrn_tags_index ON tags
  USING pgroonga (id,
                  names pgroonga_text_array_term_search_ops_v2,
                  readings pgroonga_text_array_term_search_ops_v2);

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

EXPLAIN (COSTS OFF)
SELECT names, pgroonga_score(tags)
  FROM tags
  WHERE names &^ 'Groon' OR
        readings &^~ 'posu';

SELECT names, pgroonga_score(tags)
  FROM tags
  WHERE names &^ 'Groon' OR
        readings &^~ 'posu';

DROP TABLE tags;
