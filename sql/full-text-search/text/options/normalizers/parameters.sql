CREATE TABLE memos (
  tags text
);

INSERT INTO memos VALUES ('グル∙ンガ');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (tags)
  WITH (normalizers = 'NormalizerNFKC130("unify_kana", true),
                       NormalizerNFKC130("unify_middle_dot", true)');

SELECT jsonb_pretty(
  pgroonga_command('table_tokenize',
                   ARRAY[
                     'table', 'Lexicon' || 'pgrn_index'::regclass::oid || '_0',
                     'string', 'グル∙ンガ',
                     'mode', 'GET',
                     'command_version', '3'
                   ])::jsonb->'body'
);

DROP TABLE memos;
