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

EXPLAIN (COSTS OFF)
SELECT title, content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE ARRAY[title, content] &@~
       ('りんご',
        ARRAY[5, 2],
        'pgrn_index')::pgroonga_full_text_search_condition;

SELECT title, content, pgroonga_score(tableoid, ctid)
  FROM memos
 WHERE ARRAY[title, content] &@~
       ('りんご',
        ARRAY[5, 2],
        'pgrn_index')::pgroonga_full_text_search_condition;

DROP TABLE memos;
