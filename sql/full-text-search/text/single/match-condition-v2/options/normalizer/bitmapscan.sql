CREATE TABLE memos (
  content text
);

INSERT INTO memos VALUES ('りんご');
INSERT INTO memos VALUES ('リンゴ');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (content pgroonga_text_full_text_search_ops_v2)
  WITH (normalizer = 'NormalizerNFKC100("unify_kana", true)');

SET enable_seqscan = off;
SET enable_indexscan = off;
SET enable_bitmapscan = on;

\pset format unaligned
EXPLAIN (COSTS OFF)
SELECT content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@
       ('りんご',
        ARRAY[5],
        'pgrn_index')::pgroonga_full_text_search_condition
\g |sed -r -e "s/('.+'|ROW.+)::pgroonga/pgroonga/g"
\pset format aligned

SELECT content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@
       ('りんご',
        ARRAY[5],
        'pgrn_index')::pgroonga_full_text_search_condition;

DROP TABLE memos;
