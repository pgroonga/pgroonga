CREATE TABLE memos (
  id integer,
  content text
);

INSERT INTO memos VALUES (1, 'PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES (2, 'Groonga is fast full text search engine.');
INSERT INTO memos VALUES (3, 'PGroonga is a PostgreSQL extension that uses Groonga.');

CREATE INDEX pgroonga_index ON memos
  USING pgroonga (content pgroonga.text_full_text_search_ops);

SELECT pgroonga.flush('pgroonga_index');

DROP TABLE memos;
