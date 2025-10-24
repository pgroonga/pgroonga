-- Only test when `PGRN_LANGUAGE_MODEL_TEST` is set.
\getenv language_model_test PGRN_LANGUAGE_MODEL_TEST
SELECT NOT :{?language_model_test} AS omit \gset
\if :omit
  \quit
\endif

CREATE TABLE memos (
  content text
);

INSERT INTO memos VALUES ('I am a king.');
INSERT INTO memos VALUES ('I am a queen.');

SELECT array_length(pgroonga_language_model_vectorize(
  'hf:///groonga/all-MiniLM-L6-v2-Q4_K_M-GGUF',
  content), 1)
FROM memos;

DROP TABLE memos;
