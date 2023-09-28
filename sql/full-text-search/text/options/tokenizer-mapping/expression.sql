CREATE TABLE memos (
  title text,
  content text
);

CREATE INDEX pgrn_index ON memos
  USING pgroonga (title,
                  ('content0 ' || content),
                  ('content1 ' || content))
  WITH (tokenizer = 'TokenNgram',
        tokenizer_mapping = '{
          "expr": "TokenNgram(\"n\", 3)",
          "expr1": "TokenNgram(\"n\", 4)"
        }');

\out null
SELECT
  'Lexicon' || 'pgrn_index'::regclass::oid || '_0' as title_lexicon_name,
  'Lexicon' || 'pgrn_index'::regclass::oid || '_1' as content0_lexicon_name,
  'Lexicon' || 'pgrn_index'::regclass::oid || '_2' as content1_lexicon_name;
\out
\gset

SELECT pgroonga_command('schema')::jsonb#>'{1,tables}'
         ->:'title_lexicon_name'
         #>'{command,arguments,default_tokenizer}';
SELECT pgroonga_command('schema')::jsonb#>'{1,tables}'
         ->:'content0_lexicon_name'
         #>'{command,arguments,default_tokenizer}';
SELECT pgroonga_command('schema')::jsonb#>'{1,tables}'
         ->:'content1_lexicon_name'
         #>'{command,arguments,default_tokenizer}';

DROP TABLE memos;
