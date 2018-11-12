CREATE TABLE memos (
  id integer,
  content text
);

INSERT INTO memos VALUES (1, 'りんご');
INSERT INTO memos VALUES (2, 'リンゴ');
INSERT INTO memos VALUES (3, '林檎');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (content)
  WITH (token_filters = 'TokenFilterNFKC100("unify_kana", true)');

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT id, content
  FROM memos
 WHERE content &@ 'りんご';

SELECT id, content
  FROM memos
 WHERE content &@ 'りんご';

DROP TABLE memos;
