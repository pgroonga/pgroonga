SET pgroonga.enable_wal = yes;

CREATE TABLE memos (
  content text
);

INSERT INTO memos
SELECT clock_timestamp()::text
  FROM generate_series(1, 50000);

CREATE INDEX pgrn_memos_index ON memos USING PGroonga (content);
SELECT pgroonga_wal_truncate();

DROP TABLE memos;
