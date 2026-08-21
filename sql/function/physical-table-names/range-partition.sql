CREATE TABLE blogs (
  title text,
  content text,
  registered_at date
) PARTITION BY RANGE (registered_at);

CREATE TABLE blogs_2023 PARTITION OF blogs
FOR VALUES FROM ('2023-01-01') TO ('2023-12-31');
CREATE TABLE blogs_2024 PARTITION OF blogs
FOR VALUES FROM ('2024-01-01') TO ('2024-12-31');
CREATE TABLE blogs_2025 PARTITION OF blogs
FOR VALUES FROM ('2025-01-01') TO ('2025-12-31');

INSERT INTO blogs (title, content, registered_at)
VALUES
('Hello 2023!!!', 'A happy new year', '2023-01-01 11:32:00'),
('Hello Groonga', 'Gronga is fast!', '2023-07-20 14:48:00'),
('Hello 2024!!!', 'Welcome to 2024!', '2024-01-01 09:38:00'),
('Hello Mroonga', 'Mroonga is a storage engine for MySQL', '2024-09-11 12:45:00'),
('Hello 2025!!!', 'Wishing you a happy New Year', '2025-01-01 08:39:00'),
('Hello PGroonga', 'PGroonga is a PostgreSQL extension to use Groonga as the index', '2025-12-25 16:47:00');

CREATE INDEX pgroonga_content_index ON blogs USING pgroonga (content);

SELECT pgroonga_physical_table_names('pgroonga_content_index', 'shard') = ARRAY(
  SELECT 'Sources' || pg_class.relfilenode::text
    FROM pg_class
   WHERE pg_class.oid IN('blogs_2023_content_idx'::regclass,
                         'blogs_2024_content_idx'::regclass,
                         'blogs_2025_content_idx'::regclass)
ORDER BY pg_class.oid);

DROP TABLE blogs;
