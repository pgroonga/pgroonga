CREATE TABLE memos (
  content varchar(4097)
);

CREATE INDEX pgrn_index ON memos USING pgroonga (content);

DROP TABLE memos;
