/* Temporary test to be used during implementation of a custom scan. */

CREATE TABLE memos (content text);
CREATE INDEX memos_content ON memos USING pgroonga (content);
INSERT INTO memos VALUES ('PGroonga');

SET pgroonga.enable_custom_scan = on;

EXPLAIN (COSTS OFF) SELECT * FROM memos;
SELECT * FROM memos;

DROP TABLE memos;
