CREATE TABLE memos (
  tags text
);

INSERT INTO memos VALUES ('グルンガ');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (tags)
  WITH (normalizer = 'NormalizerNFKC100("unify_kana", true)');

SELECT jsonb_pretty(
  pgroonga_command('table_tokenize',
                   ARRAY[
                     'table', 'Lexicon' || 'pgrn_index'::regclass::oid || '_0',
                     'string', 'グルンガ',
                     'mode', 'GET',
                     'command_version', '3'
                   ])::jsonb->'body'
);

DROP TABLE memos;
