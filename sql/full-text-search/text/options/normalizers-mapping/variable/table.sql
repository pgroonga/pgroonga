CREATE TABLE normalizations (
  target text,
  normalized text
);

CREATE INDEX pgrn_normalizations_index ON normalizations
 USING pgroonga (target pgroonga_text_term_search_ops_v2,
                 normalized);

INSERT INTO normalizations VALUES ('ぐ', 'く');
INSERT INTO normalizations VALUES ('が', 'か');

CREATE TABLE memos (
  content text
);

INSERT INTO memos VALUES ('グルンガ');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (content)
  WITH (normalizers_mapping = '{
          "content": "NormalizerNFKC130(\"unify_kana\", true), NormalizerTable(\"normalized\", \"${table:pgrn_normalizations_index}.normalized\", \"target\", \"target\")"
       }');

SELECT jsonb_pretty(
  pgroonga_command('select',
                   ARRAY[
                     'table', 'Lexicon' || 'pgrn_index'::regclass::oid || '_0',
                     'limit', '-1',
                     'sort_keys', '_key',
                     'output_columns', '_key',
                     'command_version', '3'
                   ])::jsonb->'body'->'records'
);

DROP TABLE normalizations;
DROP TABLE memos;
