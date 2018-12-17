require_relative "helpers/sandbox"

class StreamingReplicationTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  setup :setup_slave_db
  teardown :teardown_slave_db

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
                 run_sql_slave("EXPLAIN (COSTS OFF) #{select};"))
    output = <<-OUTPUT
#{select};
      content      
-------------------
 PGroonga is good!
(1 row)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_slave("#{select};"))
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
                 run_sql_slave("#{select};"))
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
                 run_sql_slave("#{select};"))
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
                 run_sql_slave("#{select};"))
  end

  test "pgroonga_vacuum" do
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")

    run_sql_slave("SELECT pgroonga_wal_apply();")
    pgroonga_table_name_sql = "SELECT pgroonga_table_name('memos_content');"
    pgroonga_table_name =
      run_sql_slave(pgroonga_table_name_sql)[0].scan(/Sources\d+/)[0]

    run_sql("REINDEX INDEX memos_content;")
    run_sql_slave("SELECT pgroonga_wal_apply();")

    pgroonga_table_exist_sql =
      "SELECT pgroonga_command('object_exist #{pgroonga_table_name}')" +
      "::json->>1 AS exist;"
    assert_equal([<<-OUTPUT, ""], run_sql_slave(pgroonga_table_exist_sql))
#{pgroonga_table_exist_sql}
 exist 
-------
 true
(1 row)

    OUTPUT

    assert_equal([<<-OUTPUT, ""], run_sql_slave("SELECT pgroonga_vacuum();"))
SELECT pgroonga_vacuum();
 pgroonga_vacuum 
-----------------
 t
(1 row)

    OUTPUT

    assert_equal([<<-OUTPUT, ""], run_sql_slave(pgroonga_table_exist_sql))
#{pgroonga_table_exist_sql}
 exist 
-------
 false
(1 row)

    OUTPUT
  end
end
