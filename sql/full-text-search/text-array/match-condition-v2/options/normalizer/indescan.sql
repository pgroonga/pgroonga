CREATE TABLE memos (
  title text,
  content text
);

INSERT INTO memos VALUES ('ひらがな', 'りんご');
INSERT INTO memos VALUES ('カタカナ', 'リンゴ');

CREATE INDEX pgrn_index ON memos
 USING pgroonga ((ARRAY[title, content])
                 pgroonga_text_array_full_text_search_ops_v2)
  WITH (normalizer = 'NormalizerNFKC100("unify_kana", true)');

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT title, content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE ARRAY[title, content] &@
         pgroonga_condition(
           'りんご',
           ARRAY[5, 2],
           index_name => 'pgrn_index'
         )
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g"
\pset format aligned

SELECT title, content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE ARRAY[title, content] &@
         pgroonga_condition(
           'りんご',
           ARRAY[5, 2],
           index_name => 'pgrn_index'
         );

DROP TABLE memos;
