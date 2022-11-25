require_relative "helpers/sandbox"

class StreamingReplicationTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  setup :setup_standby_db
  teardown :teardown_standby_db

  test "index search" do
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")

    select = "SELECT * FROM memos WHERE content &@ 'PGroonga'"
    output = <<-OUTPUT
EXPLAIN (COSTS OFF) #{select};
                    QUERY PLAN                     
---------------------------------------------------
 Bitmap Heap Scan on memos
   Recheck Cond: (content &@ 'PGroonga'::text)
   ->  Bitmap Index Scan on memos_content
         Index Cond: (content &@ 'PGroonga'::text)
(4 rows)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("EXPLAIN (COSTS OFF) #{select};"))
    output = <<-OUTPUT
#{select};
      content      
-------------------
 PGroonga is good!
(1 row)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{select};"))
  end

  test "tokenizer options" do
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos " +
            "USING pgroonga (content) " +
            "WITH (tokenizer='TokenNgram(\"unify_alphabet\", false)');")
    run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")

    select = "SELECT * FROM memos WHERE content &@ 'Groonga'"
    output = <<-OUTPUT
#{select};
      content      
-------------------
 PGroonga is good!
(1 row)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{select};"))
  end

  test "normalizer options" do
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos " +
            "USING pgroonga (content) " +
            "WITH (normalizer='NormalizerNFKC100(\"unify_kana\", true)');")
    run_sql("INSERT INTO memos VALUES ('りんご');")
    run_sql("INSERT INTO memos VALUES ('リンゴ');")
    run_sql("INSERT INTO memos VALUES ('林檎');")

    select = "SELECT * FROM memos WHERE content &@ 'りんご'"
    output = <<-OUTPUT
#{select};
 content 
---------
 りんご
 リンゴ
(2 rows)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{select};"))
  end

  test "token filters options" do
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos " +
            "USING pgroonga (content) " +
            "WITH (token_filters='TokenFilterNFKC100(\"unify_kana\", true)');")
    run_sql("INSERT INTO memos VALUES ('りんご');")
    run_sql("INSERT INTO memos VALUES ('リンゴ');")
    run_sql("INSERT INTO memos VALUES ('林檎');")

    select = "SELECT * FROM memos WHERE content &@ 'りんご'"
    output = <<-OUTPUT
#{select};
 content 
---------
 りんご
 リンゴ
(2 rows)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{select};"))
  end

  test "pgroonga_vacuum" do
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")

    run_sql_standby("SELECT pgroonga_wal_apply();")
    pgroonga_table_name_sql = "SELECT pgroonga_table_name('memos_content');"
    pgroonga_table_name =
      run_sql_standby(pgroonga_table_name_sql)[0].scan(/Sources\d+/)[0]

    run_sql("REINDEX INDEX memos_content;")
    run_sql_standby("SELECT pgroonga_wal_apply();")

    pgroonga_table_exist_sql =
      "SELECT pgroonga_command('object_exist #{pgroonga_table_name}')" +
      "::json->>1 AS exist;"
    assert_equal([<<-OUTPUT, ""], run_sql_standby(pgroonga_table_exist_sql))
#{pgroonga_table_exist_sql}
 exist 
-------
 true
(1 row)

    OUTPUT

    assert_equal([<<-OUTPUT, ""], run_sql_standby("SELECT pgroonga_vacuum();"))
SELECT pgroonga_vacuum();
 pgroonga_vacuum 
-----------------
 t
(1 row)

    OUTPUT

    assert_equal([<<-OUTPUT, ""], run_sql_standby(pgroonga_table_exist_sql))
#{pgroonga_table_exist_sql}
 exist 
-------
 false
(1 row)

    OUTPUT
  end

  sub_test_case "pgroonga_wal_applier" do
    def shared_preload_libraries_standby
      ["pgroonga_wal_applier"]
    end

    def naptime
      1
    end

    def additional_standby_configurations
      "pgroonga_wal_applier.naptime = #{naptime}"
    end

    test "auto apply" do
      run_sql("CREATE TABLE memos (content text);")
      run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
      run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")

      sleep(naptime)

      sql = <<-SQL
SELECT jsonb_pretty(
    pgroonga_command('select',
                     ARRAY[
                       'table', pgroonga_table_name('memos_content')
                     ])::jsonb->1
  ) AS select
      SQL
      assert_equal([<<-OUTPUT, ""], run_sql_standby(sql))
#{sql}
             select              
---------------------------------
 [                              +
     [                          +
         [                      +
             1                  +
         ],                     +
         [                      +
             [                  +
                 "_id",         +
                 "UInt32"       +
             ],                 +
             [                  +
                 "_key",        +
                 "UInt64"       +
             ],                 +
             [                  +
                 "content",     +
                 "LongText"     +
             ]                  +
         ],                     +
         [                      +
             1,                 +
             1,                 +
             "PGroonga is good!"+
         ]                      +
     ]                          +
 ]
(1 row)

      OUTPUT
    end
  end

  sub_test_case "pgroonga.max_wal_size" do
    def additional_configurations
      "pgroonga.max_wal_size = 32kB"
    end

    def additional_standby_configurations
      "pgroonga_wal_applier.naptime = 1800"
    end

    test "rotated" do
      run_sql("CREATE TABLE memos (title text, content text);")
      run_sql("CREATE INDEX memos_content ON memos " +
              "USING pgroonga (title, content);")
      100.times do |i|
        run_sql("INSERT INTO memos VALUES " +
                "('#{i}KiB', '#{(i % 10).to_s * 1024}');")
        run_sql_standby("SELECT pgroonga_wal_apply() as set#{i}")
      end

      sql = <<-SQL
SELECT title FROM memos WHERE content &@~ '0'
      SQL
      assert_equal([<<-OUTPUT, ""], run_sql_standby(sql))
#{sql}
 title 
-------
 0KiB
 10KiB
 20KiB
 30KiB
 40KiB
 50KiB
 60KiB
 70KiB
 80KiB
 90KiB
(10 rows)

      OUTPUT
    end
  end

  sub_test_case "pgroonga_standby_maintainer" do
    def shared_preload_libraries_standby
      ["pgroonga_standby_maintainer"]
    end

    def naptime
      1
    end

    def additional_standby_configurations
      "pgroonga_standby_maintainer.naptime = #{naptime}"
    end

    sub_test_case "auto apply" do
      test "no partition" do
        run_sql("CREATE TABLE memos (content text);")
        run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
        run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")

        sleep(naptime)

        sql = <<-SQL
SELECT jsonb_pretty(
    pgroonga_command('select',
                     ARRAY[
                       'table', pgroonga_table_name('memos_content')
                     ])::jsonb->1
  ) AS select
        SQL
        assert_equal([<<-OUTPUT, ""], run_sql_standby(sql))
#{sql}
             select              
---------------------------------
 [                              +
     [                          +
         [                      +
             1                  +
         ],                     +
         [                      +
             [                  +
                 "_id",         +
                 "UInt32"       +
             ],                 +
             [                  +
                 "_key",        +
                 "UInt64"       +
             ],                 +
             [                  +
                 "content",     +
                 "LongText"     +
             ]                  +
         ],                     +
         [                      +
             1,                 +
             1,                 +
             "PGroonga is good!"+
         ]                      +
     ]                          +
 ]
(1 row)

        OUTPUT
      end

      test "partition" do
        run_sql(<<-SQL)
CREATE TABLE cities (
  city_code varchar(5) NOT NULL,
  summary text
) PARTITION BY LIST (city_code)
        SQL
        run_sql("CREATE INDEX summary_index ON cities USING pgroonga (summary);")
        run_sql(<<-SQL)
CREATE TABLE cities_20_01 PARTITION OF cities FOR VALUES IN ('20-01')
        SQL
        run_sql("INSERT INTO cities_20_01 VALUES ('20-01','Osaka');")

        sleep(naptime)

        sql = <<-SQL
SELECT jsonb_pretty(
    pgroonga_command('select',
                     ARRAY[
                       'table', pgroonga_table_name('cities_20_01_summary_idx')
                     ])::jsonb->1
  ) AS select
        SQL
        assert_equal([<<-OUTPUT, ""], run_sql_standby(sql))
#{sql}
           select           
----------------------------
 [                         +
     [                     +
         [                 +
             1             +
         ],                +
         [                 +
             [             +
                 "_id",    +
                 "UInt32"  +
             ],            +
             [             +
                 "_key",   +
                 "UInt64"  +
             ],            +
             [             +
                 "summary",+
                 "LongText"+
             ]             +
         ],                +
         [                 +
             1,            +
             1,            +
             "Osaka"       +
         ]                 +
     ]                     +
 ]
(1 row)

        OUTPUT
      end
    end

    sub_test_case "auto pgroonga_vacuum" do
      test "no partition" do
        run_sql("CREATE TABLE memos (content text);")
        run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
        run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")

        sleep(naptime)

        pgroonga_table_name_sql = "SELECT pgroonga_table_name('memos_content');"
        pgroonga_table_name =
          run_sql_standby(pgroonga_table_name_sql)[0].scan(/Sources\d+/)[0]

        run_sql("REINDEX INDEX memos_content;")
        run_sql_standby("SELECT pgroonga_wal_apply();")

        pgroonga_table_exist_sql =
          "SELECT pgroonga_command('object_exist #{pgroonga_table_name}')" +
          "::json->>1 AS exist;"
        assert_equal([<<-OUTPUT, ""], run_sql_standby(pgroonga_table_exist_sql))
#{pgroonga_table_exist_sql}
 exist 
-------
 true
(1 row)

         OUTPUT

        sleep(naptime)

        assert_equal([<<-OUTPUT, ""], run_sql_standby(pgroonga_table_exist_sql))
#{pgroonga_table_exist_sql}
 exist 
-------
 false
(1 row)

        OUTPUT
      end

      test "partition" do
        run_sql(<<-SQL)
CREATE TABLE cities (
  city_code varchar(5) NOT NULL,
  summary text
) PARTITION BY LIST (city_code)
        SQL
        run_sql("CREATE INDEX summary_index ON cities USING pgroonga (summary);")
        run_sql(<<-SQL)
CREATE TABLE cities_20_01 PARTITION OF cities FOR VALUES IN ('20-01')
        SQL
        run_sql("INSERT INTO cities_20_01 VALUES ('20-01','Osaka');")

        sleep(naptime)

        pgroonga_table_name_sql =
          "SELECT pgroonga_table_name('cities_20_01_summary_idx');"
        pgroonga_table_name =
          run_sql_standby(pgroonga_table_name_sql)[0].scan(/Sources\d+/)[0]

        # We can't use REINDEX INDEX for partition table with
        # PostgreSQL 13 or earlier.
        # run_sql("REINDEX INDEX summary_index;")
        run_sql("REINDEX INDEX cities_20_01_summary_idx;")
        run_sql_standby("SELECT pgroonga_wal_apply();")

        pgroonga_table_exist_sql =
          "SELECT pgroonga_command('object_exist #{pgroonga_table_name}')" +
          "::json->>1 AS exist;"
        assert_equal([<<-OUTPUT, ""], run_sql_standby(pgroonga_table_exist_sql))
#{pgroonga_table_exist_sql}
 exist 
-------
 true
(1 row)

        OUTPUT

        sleep(naptime)

        assert_equal([<<-OUTPUT, ""], run_sql_standby(pgroonga_table_exist_sql))
#{pgroonga_table_exist_sql}
 exist 
-------
 false
(1 row)

        OUTPUT
      end
    end
  end
end
