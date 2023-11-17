CREATE EXTENSION IF NOT EXISTS postgres_fdw;

CREATE SERVER remote_server
    FOREIGN DATA WRAPPER postgres_fdw
    OPTIONS (host 'localhost', port '5432', dbname 'remote_database');

CREATE FOREIGN TABLE synonym_groups (
  synonyms text[]
) SERVER remote_server;

SELECT pgroonga_query_expand('synonym_groups',
                             'synonyms',
                             'synonyms',
                             'groonga');

DROP TABLE synonym_groups;
