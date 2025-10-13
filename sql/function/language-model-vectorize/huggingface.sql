CREATE TABLE memos (
  content text
);

INSERT INTO memos VALUES ('I am a king.');
INSERT INTO memos VALUES ('I am a queen.');

SELECT (pgroonga_language_model_vectorize(
  'hf:///groonga/bge-m3-Q4_K_M-GGUF',
  content))[1:3]
FROM memos;

DROP TABLE memos;
