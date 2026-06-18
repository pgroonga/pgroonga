CREATE TABLE memos (
  id integer,
  title text,
  content text
);

INSERT INTO memos VALUES
  (1,
   'PostgreSQLはリレーショナル・データベース管理システムです。',
   'すごいでしょう');
INSERT INTO memos VALUES
  (2,
   'Groongaは日本語対応の高速な全文検索エンジンです。',
   'スゴイデショウ');
INSERT INTO memos VALUES
  (3,
   'PGroongaはインデックスとしてGroongaを使うためのPostgreSQLの拡張機能です。',
   'ハバナイスデー');
INSERT INTO memos VALUES
  (4,
   'groongaコマンドがあります。',
   '今日はコンバンワこんにちわ');

CREATE INDEX pgroonga_title_search_index ON memos USING pgroonga (title)
  WITH (
    normalizers = 'NormalizerNFKC150(
                     "unify_to_romaji", true,
                     "unify_hyphen_and_prolonged_sound_mark", true
                   )',
    tokenizer='TokenNgram(
                 "unify_alphabet", false,
                 "unify_symbol", false,
                 "unify_digit", false,
                 "report_source_location", true
               )'
  );

CREATE INDEX pgroonga_content_search_index ON memos USING pgroonga (content)
  WITH (
    normalizers = 'NormalizerNFKC150(
                     "unify_to_romaji", true,
                     "unify_hyphen_and_prolonged_sound_mark", true
                   )',
    tokenizer='TokenNgram(
                 "unify_alphabet", false,
                 "unify_symbol", false,
                 "unify_digit", false,
                 "report_source_location", true
               )'
  );

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

\pset format unaligned
SELECT id, content
  FROM memos
 WHERE content &@ 'すごい'
 ORDER BY id;

SELECT token->>'value' AS value,
       token->>'position' AS position,
       token->>'in_lexicon' AS in_lexicon
  FROM jsonb_array_elements(
    pgroonga_explain_match('pgroonga_content_search_index', 'すごい')->'tokens'
  ) AS token;

SELECT jsonb_array_elements_text(
  pgroonga_explain_match('pgroonga_content_search_index', 'すごい')->'matched_terms'
);

SELECT id, content
  FROM memos
 WHERE title &@~ 'デス'
 ORDER BY id;

SELECT token->>'value' AS value,
       token->>'position' AS position,
       token->>'in_lexicon' AS in_lexicon
  FROM jsonb_array_elements(
    pgroonga_explain_match('pgroonga_title_search_index', 'デス')->'tokens'
  ) AS token;

SELECT jsonb_array_elements_text(
  pgroonga_explain_match('pgroonga_title_search_index', 'デス')->'matched_terms'
);
\pset format aligned

DROP TABLE memos;
