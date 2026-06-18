CREATE TABLE memos (
  tags text
);

INSERT INTO memos VALUES ('PGroonga is fast');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (tags)
  WITH (tokenizer = 'TokenNgram("n", 3)',
        normalizer = '');

SELECT jsonb_pretty(
  pgroonga_command('table_tokenize',
                   ARRAY[
                     'table', 'Lexicon' || 'pgrn_index'::regclass::oid || '_0',
                     'string', 'PGroonga is fast',
                     'mode', 'GET',
                     'command_version', '3'
                   ])::jsonb->'body'
);

DROP TABLE memos;
