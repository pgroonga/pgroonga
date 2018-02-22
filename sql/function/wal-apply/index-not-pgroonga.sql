CREATE TABLE memos (
  id integer
);

CREATE INDEX index ON memos (id);

SELECT pgroonga_wal_apply('index');

DROP TABLE memos;
