CREATE TABLE memos (
  title text,
  tags text
);

CREATE INDEX pgrn_index ON memos
 USING pgroonga (title, tags)
  WITH (tokenizer_mapping = '{
          "tags": "TokenNgram(\"loose_symbol\", true, \"report_source_location\", true)"
        }',
        normalizers_mapping = '{
          "tags": "NormalizerNFKC100(\"unify_kana\", true)"
        }');

SELECT pgroonga_highlight_html(
  'この八百屋のリンゴはおいしい。' ||
  '連絡先は０９０-12345678だそうだ。',
  ARRAY['りんご', '（090）1234５６７８'],
  'pgrn_index.nonexistent');

DROP TABLE memos;
