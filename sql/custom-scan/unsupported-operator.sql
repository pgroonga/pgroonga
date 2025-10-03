CREATE TABLE tags (
    tag varchar(200)
);

INSERT INTO tags VALUES ('Fuji');
INSERT INTO tags VALUES ('Hawk');
INSERT INTO tags VALUES ('Eggplant');

CREATE INDEX tag_index ON tags USING pgroonga (tag);

SET pgroonga.enable_custom_scan = on;

EXPLAIN (COSTS OFF)
SELECT tag
  FROM tags
 WHERE tag &@~ 'fuji';

SELECT tag
  FROM tags
 WHERE tag &@~ 'fuji';

DROP TABLE tags;
