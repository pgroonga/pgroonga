# News

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
