CREATE TABLE memos (
  id integer,
  record jsonb
);

CREATE INDEX pgrn_index ON memos
 USING pgroonga (record pgroonga_jsonb_full_text_search_ops_v2)
  WITH (lexicon_flags_mapping = '{
	  "record": ["LARGE"]
	}');

\out null
SELECT
  'Lexicon' || 'pgrn_index'::regclass::oid || '_0' as record_lexicon_name;
\out
\gset

SELECT pgroonga_command('schema')::jsonb#>'{1,tables}'
         ->:'record_lexicon_name'
         #>'{command,arguments,flags}';

DROP TABLE memos;
