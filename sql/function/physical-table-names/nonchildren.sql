CREATE TABLE blogs (
  title text,
  content text,
  registered_at date
) PARTITION BY RANGE (registered_at);

CREATE INDEX pgroonga_content_index ON blogs USING pgroonga (content);

SELECT pgroonga_physical_table_names('pgroonga_content_index',
                                     'shard') AS physical_tables;

DROP TABLE blogs;
