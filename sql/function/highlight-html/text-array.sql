CREATE TABLE memos (
  contents text[]
);

CREATE INDEX pgrn_index ON memos
 USING pgroonga (contents)
  WITH (tokenizer = 'TokenNgram("report_source_location", true)',
        normalizer = 'NormalizerNFKC130');

SELECT pgroonga_highlight_html(
  ARRAY['one two three', NULL, 'five', 'six three'],
  ARRAY['two three', 'six'],
  'pgrn_index');

DROP TABLE memos;
