CREATE TABLE メモ (
  id integer,
  コンテンツ text
);

INSERT INTO メモ VALUES (1, 'PostgreSQLはRDBMSです。');
INSERT INTO メモ VALUES (2, 'Groongaは高速な全文検索エンジンです。');
INSERT INTO メモ VALUES (3, 'PGroongaはGroongaを使うPostgreSQLの拡張機能です。');

CREATE INDEX 全文検索索引 ON メモ
  USING pgroonga (コンテンツ pgroonga.text_full_text_search_ops);

SET enable_seqscan = off;
SET enable_indexscan = on;
SET enable_bitmapscan = off;

SELECT id, コンテンツ
  FROM メモ
 WHERE コンテンツ %% '全文検索';

DROP TABLE メモ;
