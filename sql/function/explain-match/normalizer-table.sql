CREATE TABLE normalizations (
  target text,
  normalized text
);

CREATE INDEX pgrn_normalizations_index ON normalizations
 USING pgroonga (target pgroonga_text_term_search_ops_v2,
                 normalized);

INSERT INTO normalizations VALUES ('齋', '斉');
INSERT INTO normalizations VALUES ('斎', '斉');
INSERT INTO normalizations VALUES ('渡邉', '渡辺');
INSERT INTO normalizations VALUES ('渡邊', '渡辺');
INSERT INTO normalizations VALUES ('渡部', '渡辺');

CREATE TABLE memos (
  id integer,
  content text
);

INSERT INTO memos VALUES (1, '斉藤');
INSERT INTO memos VALUES (2, '渡辺');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (content)
  WITH (normalizers =
          'NormalizerTable("normalized",
                           "${table:public.pgrn_normalizations_index}.normalized",
                           "target", "target")',
        tokenizer = 'TokenNgram("report_source_location", true)');

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

\pset format unaligned
SELECT id, content
  FROM memos
 WHERE content &@ '齋藤'
 ORDER BY id;

SELECT token->>'source' AS source,
       token->>'value' AS value,
       token->>'position' AS position,
       token->>'in_lexicon' AS in_lexicon
  FROM jsonb_array_elements(
    pgroonga_explain_match('pgrn_index', '齋藤')->'tokens'
  ) AS token;

SELECT jsonb_array_elements_text(
  pgroonga_explain_match('pgrn_index', '齋藤')->'matched_terms'
);

SELECT id, content
  FROM memos
 WHERE content &@ '渡邉'
 ORDER BY id;

SELECT token->>'source' AS source,
       token->>'value' AS value,
       token->>'position' AS position,
       token->>'in_lexicon' AS in_lexicon
  FROM jsonb_array_elements(
    pgroonga_explain_match('pgrn_index', '渡邉')->'tokens'
  ) AS token;

SELECT id, content
  FROM memos
 WHERE content &@ '渡邊'
 ORDER BY id;

SELECT token->>'source' AS source,
       token->>'value' AS value,
       token->>'position' AS position,
       token->>'in_lexicon' AS in_lexicon
  FROM jsonb_array_elements(
    pgroonga_explain_match('pgrn_index', '渡邊')->'tokens'
  ) AS token;

SELECT id, content
  FROM memos
 WHERE content &@ '渡部'
 ORDER BY id;

SELECT token->>'source' AS source,
       token->>'value' AS value,
       token->>'position' AS position,
       token->>'in_lexicon' AS in_lexicon
  FROM jsonb_array_elements(
    pgroonga_explain_match('pgrn_index', '渡部')->'tokens'
  ) AS token;

SELECT unnest(pgroonga_normalizer_table_variants('pgrn_index', '斉藤'));

SELECT unnest(pgroonga_normalizer_table_variants('pgrn_index', '渡辺'));
\pset format aligned

DROP TABLE normalizations;
DROP TABLE memos;
