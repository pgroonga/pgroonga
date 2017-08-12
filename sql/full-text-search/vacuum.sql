-- Just for reducing disk usage. This tests nothing.

CREATE TABLE memos (content text);
CREATE INDEX pgroonga_index ON memos USING pgroonga (content);
INSERT INTO memos VALUES ('content');

VACUUM;

DROP TABLE memos;
