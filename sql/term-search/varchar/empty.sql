CREATE TABLE memos (
  content varchar
);

CREATE INDEX pgrn_index ON memos USING pgroonga (content);

INSERT INTO memos VALUES ('');
INSERT INTO memos VALUES (E'\x0a');

DROP TABLE memos;
