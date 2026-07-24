CREATE TABLE fruits (
  id int,
  items jsonb
);

CREATE INDEX pgroonga_index ON fruits
 USING pgroonga (items pgroonga_jsonb_ops_v2)
  WITH (lexicon_flags_mapping = '{
	  "items": ["LARGE"]
	}');

\out null
SELECT
  'JSONValueLexiconFullTextSearch' || 'pgroonga_index'::regclass::oid || '_0' as items_json_value_lexicon_name;
\out
\gset

SELECT pgroonga_command('schema')::jsonb#>'{1,tables}'
         ->:'items_json_value_lexicon_name'
         #>'{command,arguments,flags}';

DROP TABLE fruits;
