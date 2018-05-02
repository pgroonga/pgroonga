CREATE TABLE memos (
  content varchar(256)
);

INSERT INTO memos VALUES ('りんご');
INSERT INTO memos VALUES ('リンゴ');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (content pgroonga_varchar_full_text_search_ops_v2)
  WITH (normalizer = 'NormalizerNFKC100("unify_kana", true)');

SET enable_seqscan = on;
SET enable_indexscan = off;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@
       ('りんご',
        ARRAY[5],
        'pgrn_index')::pgroonga_full_text_search_condition;

SELECT content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE content &@
       ('りんご',
        ARRAY[5],
        'pgrn_index')::pgroonga_full_text_search_condition;

DROP TABLE memos;
