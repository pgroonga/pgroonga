# News

## 0.8.0: 2015-09-01

You can upgrade to 0.6.0 from 0.5.0 by override install. You don't need to re-create `pgroonga` indexes.

### Improvements

  * Reduced needless loop on `VACUUM`.
  * Reduced temporary memory usage.
  * `pgroonga.log_path`: Added a variable that changes path for Groonga log.
  * incompatible: Changed the path for Groonga log to the database directory by default. You can change it by `pgroonga.log_path`.
  * `pgroonga.log_type`: Added a variable that changes how to log.
  * Supported `TRUNCATE`-ed table. [GitHub:#1] [Reported by Hiroki Nakamura]
  * `pgroonga.snippet_html()`: Added a function that generates snippet HTML. [groonga-dev,03398] [Reported by Hiroki Nakamura]
  * Supported Ubuntu 14.04 LTS (Trusty Tahr). [Suggested by Yokoda Toshiaki]
  * `pgroonga.lock_timeout`: Added a variable that changes the number of lock retries. [groonga-dev,03419] [Suggested by Naoki Takami]

### Thanks

  * Hiroki Nakamura
  * Yokoda Toshiaki
  * Naoki Takami

## 0.7.0: 2015-07-10

You can upgrade to 0.6.0 from 0.5.0 by override install. You don't need to re-create `pgroonga` indexes.

### Improvements

  * incompatible: Changed to use Groonga's default logger. Messages for PGroonga is logged to `pgroonga.log` in database directory instead of PostgreSQL's log path.
  * `pgroonga.log_level`: Added a variable that changes log level.

### Fixes

  * Fixed a bug that lexicon tables for dropped indexes aren't removed on `VACUUM`.

## 0.6.0: 2015-05-29

You can upgrade to 0.6.0 from 0.5.0 by override install. You don't need to re-create `pgroonga` indexes.

### Improvements

  * `pgroonga.score()`: Supported HOT update on PostgreSQL 9.3.
  * Supported log messages from Groonga.
  * Stopped to try opening Groonga database when Groonga database path doesn't exist.
  * Supported Debian GNU/Linux Jessie.

### Fixes

  * Fixed a bug that large block number in ctid is overflowed.

## 0.5.0: 2015-04-29

You can't upgrade to 0.5.0 from 0.4.0 without re-creating `pgroonga` index. You need to re-install PGroonga:

```sql
DROP EXTENSION pgroonga CASCADE;
CREATE EXTENSION pgroonga;
-- Create your pgroonga indexes again.
```

### Improvements

  * `pgroonga.score()`: Supported HOT update.
  * Supported Ubuntu 15.04 Vivid Vervet.
  * Supported Windows.

### Changes

  * `pgroonga.score()`: Required primary key.

## 0.4.0: 2015-03-29

You can't upgrade to 0.4.0 from 0.3.0 without re-creating `pgroonga` index. You need to re-install PGroonga:

```sql
DROP EXTENSION pgroonga CASCADE;
CREATE EXTENSION pgroonga;
-- Create your pgroonga indexes again.
```

### Improvements

  * Supported `column LIKE '%keyword'` as a short cut of `column @@ 'keyword'`.
  * Supported range search with multi-column index.
  * Added PGroonga setup script on Travis CI. Add the following line to `install` section in your `.travis.yml`:

        curl --silent --location https://github.com/pgroonga/pgroonga/raw/master/data/travis/setup.sh | sh

  * Added `pgroonga.table_name()` that returns table name in Groonga.
  * Added `pgroonga.command()` that executes Groonga command line.
  * Added `pgroonga.score()` that returns search score in Groonga.
  * Supported `timestamp` type.
  * Supported `timestamp with time zone` type.
  * Supported `varchar[]` type.
  * Supported full-text search for `text[]` type.
  * Supported full-text search by index and other search by index in one `SELECT`.
  * Added Yum repositories for CentOS 5 and 6.

### Changes

  * Dropped `text == text` search by index. Use 4096 bytes or smaller `varchar` instead.
  * Dropped PostgreSQL 9.2 support.

## 0.3.0: 2015-02-09

You can't upgrade to 0.3.0 from 0.2.0 without re-creating `pgroonga` index. You need to re-install PGroonga:

```sql
DROP EXTENSION pgroonga CASCADE;
CREATE EXTENSION pgroonga;
-- Create your pgroonga indexes again.
```

### Improvements

  * Supported encoding
  * Supported customizing tokenizer and normalizer by `WITH` such as:

    ```sql
    CREATE INDEX pgroonga_index
              ON table
           USING pgroonga (column)
            WITH (tokenizer='TokenMecab',
                  normalizer='NormalizerAuto');
    ```

  * Reduced needless locks.
  * Supported column compression by LZ4.
  * Supported non full-text search index such as text, numbers and timestamp.

### Changes

  * Changed database file base name to `pgrn` from `grn`.

## 0.2.0: 2015-01-29

The first release!!!
