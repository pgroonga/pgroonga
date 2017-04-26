CREATE TABLE memos (
  id integer,
  content text
);

INSERT INTO memos VALUES (1, 'It works.');
INSERT INTO memos VALUES (2, 'I work.');
INSERT INTO memos VALUES (3, 'I worked.');

CREATE INDEX pgrn_index ON memos
 USING pgroonga (content)
  WITH (plugins = 'token_filters/stem',
        token_filters = 'TokenFilterStem');

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT id, content
  FROM memos
 WHERE content %% 'works';

SELECT id, content
  FROM memos
 WHERE content %% 'work';

SELECT id, content
  FROM memos
 WHERE content %% 'worked';

DROP TABLE memos;
