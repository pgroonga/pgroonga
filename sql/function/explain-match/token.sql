CREATE TABLE memos (
  content text
);

INSERT INTO memos VALUES ('PGroonga');

CREATE INDEX pgrn_index ON memos
  USING pgroonga (content)
  WITH (tokenizer = 'TokenDelimit');

\pset format unaligned
SELECT token->>'value' AS value,
       token->>'position' AS position,
       token->>'in_lexicon' AS in_lexicon
  FROM jsonb_array_elements(
    pgroonga_explain_match('pgrn_index', 'pgroonga')->'tokens'
  ) AS token;

SELECT jsonb_array_elements_text(
  pgroonga_explain_match('pgrn_index', 'pgroonga')->'matched_terms'
);
\pset format aligned

DROP TABLE memos;
