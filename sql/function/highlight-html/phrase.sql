CREATE TABLE memos (
  tags text
);

CREATE INDEX pgrn_index ON memos
 USING pgroonga (tags)
  WITH (tokenizer = 'TokenNgram("report_source_location", true)',
        normalizer = 'NormalizerNFKC100');

SELECT pgroonga_highlight_html(
  'one two three four five two six three',
  ARRAY['two three', 'five'],
  'pgrn_index');

DROP TABLE memos;
