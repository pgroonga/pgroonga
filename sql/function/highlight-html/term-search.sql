-- This requires Groonga 13.0.1 or later

CREATE TABLE tags (
  name text
);

CREATE INDEX pgrn_index ON tags
  USING pgroonga (name pgroonga_text_term_search_ops_v2)
  WITH (normalizers='NormalizerNFKC130("unify_kana", true)');

SELECT pgroonga_highlight_html(
  'はろー ハロー',
  ARRAY['は'],
  'pgrn_index');

DROP TABLE tags;
