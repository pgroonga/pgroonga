SET pgroonga.enable_wal = yes;

CREATE TABLE memos (
  content text
);

CREATE TABLE tags (
  name text
);

INSERT INTO memos VALUES ('Groonga is fast!');
INSERT INTO tags VALUES ('Groonga');

CREATE INDEX pgrn_memos_index ON memos USING PGroonga (content);
CREATE INDEX pgrn_tags_index ON tags USING PGroonga (name);

INSERT INTO memos VALUES ('PGroonga is also fast!');
INSERT INTO tags VALUES ('PGroonga');

SELECT pgroonga_wal_set_applied_position(0, 0);
SELECT name, current_block, current_offset FROM pgroonga_wal_status();
SELECT pgroonga_wal_set_applied_position();

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

EXPLAIN (COSTS OFF)
SELECT *
  FROM memos
 WHERE content &@~ 'is';
SELECT *
  FROM memos
 WHERE content &@~ 'is';

EXPLAIN (COSTS OFF)
SELECT *
  FROM tags
 WHERE name &@~ 'Groonga OR PGroonga';
SELECT *
  FROM tags
 WHERE name &@~ 'Groonga OR PGroonga';

DROP TABLE tags;
DROP TABLE memos;

SET pgroonga.enable_wal = default;
