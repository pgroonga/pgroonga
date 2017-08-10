CREATE TABLE fruits (
  id int,
  items jsonb
);

CREATE INDEX pgroonga_index ON fruits
  USING pgroonga (id pgroonga.int4_ops,
                  items pgroonga.jsonb_ops);

DROP TABLE fruits;
