CREATE TABLE memos (
  title text,
  content text
);

CREATE INDEX pgrn_index ON memos
  USING pgroonga (title, content)
  WITH (index_flags_mapping = '{
	  "content": ["WITH_WEIGHT", "WEIGHT_FLOAT32"]
	}');

\out null
SELECT
  'Lexicon' || 'pgrn_index'::regclass::oid || '_0' as title_lexicon_name,
  'Lexicon' || 'pgrn_index'::regclass::oid || '_1' as content_lexicon_name;
\out
\gset

SELECT pgroonga_command('schema')::jsonb#>'{1,tables}'
         ->:'title_lexicon_name'
         #>'{columns,index,command,arguments,flags}';
SELECT pgroonga_command('schema')::jsonb#>'{1,tables}'
         ->:'content_lexicon_name'
         #>'{columns,index,command,arguments,flags}';

DROP TABLE memos;
