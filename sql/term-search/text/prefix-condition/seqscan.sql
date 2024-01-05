CREATE TABLE tags (
  name text
);

INSERT INTO tags VALUES ('たかはし');
INSERT INTO tags VALUES ('タカハシ');
INSERT INTO tags VALUES ('たかなし');

CREATE INDEX pgrn_index ON tags
  USING pgroonga (name pgroonga_text_term_search_ops_v2)
  WITH (normalizers='NormalizerNFKC130("unify_kana", true)');

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT name
  FROM tags
 WHERE name &^ pgroonga_condition('タカハ', index_name => 'pgrn_index')
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g"
\pset format aligned

SELECT name
  FROM tags
 WHERE name &^ pgroonga_condition('タカハ', index_name => 'pgrn_index');

DROP TABLE tags;
