CREATE TABLE memos (
  tags text
);

INSERT INTO memos VALUES ('グル∙ンガ');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (tags)
  WITH (normalizers = 'NormalizerNFKC130("unify_kana", true),
                       NormalizerNFKC130("unify_middle_dot", true)');

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

DROP TABLE memos;
