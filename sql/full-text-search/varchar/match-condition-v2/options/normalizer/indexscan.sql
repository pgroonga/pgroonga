CREATE TABLE memos (
  content varchar(256)
);

INSERT INTO memos VALUES ('りんご');
INSERT INTO memos VALUES ('リンゴ');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (content pgroonga_varchar_full_text_search_ops_v2)
  WITH (normalizer = 'NormalizerNFKC100("unify_kana", true)');

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@
         pgroonga_condition(
           'りんご',
           ARRAY[5],
           index_name => 'pgrn_index'
         )
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g"
\pset format aligned

SELECT content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@
         pgroonga_condition(
           'りんご',
           ARRAY[5],
           index_name => 'pgrn_index'
         );

DROP TABLE memos;
