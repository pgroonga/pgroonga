CREATE TABLE memos (
  tags text
);

CREATE INDEX pgrn_index ON memos
 USING pgroonga (tags)
  WITH (normalizer = 'NormalizerNFKC100("unify_kana", true)');

SELECT pgroonga_match_positions_character(
  'この八百屋のリンゴはおいしい。',
  ARRAY['りんご'],
  'pgrn_index');

DROP TABLE memos;
