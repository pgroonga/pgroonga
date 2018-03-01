SET pgroonga.enable_wal = true;

CREATE TABLE memos (
  content text
);

INSERT INTO memos VALUES ('Groonga is fast!');

CREATE INDEX pgrn_index ON memos USING PGroonga (content);

SELECT pgroonga_set_writable(false);

SELECT pgroonga_wal_truncate('pgrn_index');

SELECT pgroonga_set_writable(true);

SELECT pgroonga_wal_truncate('pgrn_index');

DROP TABLE memos;
