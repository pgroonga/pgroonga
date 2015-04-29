# News

## 0.5.0: 2015-04-29

You can't upgrade to 0.5.0 from 0.4.0 without re-creating `pgroonga`
index. You need to re-install PGroonga:

    DROP EXTENSION pgroonga CASCADE;
    CREATE EXTENSION pgroonga;
    -- Create your pgroonga indexes again.

### Improvements

  * `pgroonga.score()`: Supported HOT update.
  * Supported Ubuntu 15.04 Vivid Vervet.
  * Supported Windows.

### Changes

  * `pgroonga.score()`: Required primary key.

## 0.4.0: 2015-03-29

You can't upgrade to 0.4.0 from 0.3.0 without re-creating `pgroonga`
index. You need to re-install PGroonga:

    DROP EXTENSION pgroonga CASCADE;
    CREATE EXTENSION pgroonga;
    -- Create your pgroonga indexes again.

### Improvements

  * Supported `column LIKE '%keyword'` as a short cut of `column @@
    'keyword'`.
  * Supported range search with multi-column index.
  * Added PGroonga setup script on Travis CI. Add the following line
    to `install` section in your `.travis.yml`:

        curl --silent --location https://github.com/pgroonga/pgroonga/raw/master/data/travis/setup.sh | sh

  * Added `pgroonga.table_name()` that returns table name in Groonga.
  * Added `pgroonga.command()` that executes Groonga command line.
  * Added `pgroonga.score()` that returns search score in Groonga.
  * Supported `timestamp` type.
  * Supported `timestamp with time zone` type.
  * Supported `varchar[]` type.
  * Supported full-text search for `text[]` type.
  * Supported full-text search by index and other search by index in
    one `SELECT`.
  * Added Yum repositories for CentOS 5 and 6.

### Changes

  * Dropped `text == text` search by index. Use 4096 bytes or smaller
    `varchar` instead.
  * Dropped PostgreSQL 9.2 support.

## 0.3.0: 2015-02-09

You can't upgrade to 0.3.0 from 0.2.0 without re-creating `pgroonga`
index. You need to re-install PGroonga:

    DROP EXTENSION pgroonga CASCADE;
    CREATE EXTENSION pgroonga;
    -- Create your pgroonga indexes again.

### Improvements

  * Supported encoding
  * Supported customizing tokenizer and normalizer by `WITH` such as:

        CREATE INDEX pgroonga_index
                  ON table
               USING pgroonga (column)
                WITH (tokenizer='TokenMecab',
                      normalizer='NormalizerAuto');

  * Reduced needless locks.
  * Supported column compression by LZ4.
  * Supported non full-text search index such as text, numbers and
    timestamp.

### Changes

  * Changed database file base name to `pgrn` from `grn`.

## 0.2.0: 2015-01-29

The first release!!!
