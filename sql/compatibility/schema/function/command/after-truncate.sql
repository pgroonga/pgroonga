CREATE TABLE memos (
  content text
);

CREATE INDEX pgroonga_index ON memos
  USING pgroonga (content pgroonga.text_full_text_search_ops);

TRUNCATE memos;

INSERT INTO memos VALUES ('PostgreSQL is a RDBMS.');
INSERT INTO memos VALUES ('Groonga is fast full text search engine.');
INSERT INTO memos VALUES ('PGroonga is a PostgreSQL extension that uses Groonga.');

SELECT pgroonga.command('select ' ||
                        pgroonga.table_name('pgroonga_index') ||
                        ' --output_columns content')::json->>1
    AS body;

DROP TABLE memos;
