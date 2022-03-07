CREATE TABLE normalizations (
  id int PRIMARY KEY,
  target text,
  normalized text
);

CREATE INDEX normalizations_index
 ON normalizations
 USING pgroonga (target pgroonga_text_term_search_ops_v2)
 INCLUDE (normalized);
