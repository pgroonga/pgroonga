require_relative "helpers/sandbox"

class PGroongaWALResourceManagerTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  setup def check_postgresql_version
    if @postgresql.version < 15
      omit("custom WAL resource manager is available since PostgreSQL 15")
    end
  end

  def additional_configurations
    <<-CONFIG
pgroonga.enable_wal_resource_manager = yes
    CONFIG
  end

  def shared_preload_libraries
    ["pgroonga_wal_resource_manager"]
  end

  def shared_preload_libraries_standby
    []
  end

  setup :setup_standby_db
  teardown :teardown_standby_db

  setup def setup_synchronous_replication
    stop_postgres
    @postgresql.append_configuration(<<-CONFIG)
synchronous_commit = remote_apply
synchronous_standby_names = 'standby'
    CONFIG
    start_postgres
  end

  def revert_synchronous_replication
    stop_postgres
    @postgresql.append_configuration(<<-CONFIG)
synchronous_commit = on
synchronous_standby_names = ''
    CONFIG
    start_postgres
  end

  test "text" do
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

  test "text[]" do
    run_sql("CREATE TABLE memos (contents text[]);")
    run_sql("CREATE INDEX memos_contents ON memos USING pgroonga (contents);")
    run_sql("INSERT INTO memos VALUES (ARRAY['PGroonga is good!']);")
    run_sql("INSERT INTO memos VALUES (ARRAY['PostgreSQL is good!']);")
    run_sql("INSERT INTO memos VALUES (ARRAY['Groonga is good!', 'PGroonga is fast!']);")

    disable_index_scan = "SET enable_indexscan = off"
    select = "SELECT * FROM memos WHERE contents &@ 'PGroonga'"
    output = <<-OUTPUT
#{disable_index_scan};
EXPLAIN (COSTS OFF) #{select};
                     QUERY PLAN                     
----------------------------------------------------
 Bitmap Heap Scan on memos
   Recheck Cond: (contents &@ 'PGroonga'::text)
   ->  Bitmap Index Scan on memos_contents
         Index Cond: (contents &@ 'PGroonga'::text)
(4 rows)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{disable_index_scan};\n" +
                                 "EXPLAIN (COSTS OFF) #{select};"))
    output = <<-OUTPUT
#{select};
                 contents                 
------------------------------------------
 {"PGroonga is good!"}
 {"Groonga is good!","PGroonga is fast!"}
(2 rows)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{select};"))
  end

  # I don't know why we can create a PGroonga index for int[]...
  # PGroonga doesn't provide an operator class for int[]...
  test "int[]" do
    run_sql("CREATE TABLE users (scores int[]);")
    run_sql("CREATE INDEX users_scores ON users USING pgroonga (scores);")
    run_sql("INSERT INTO users VALUES (ARRAY[1, 2]);")
    run_sql("INSERT INTO users VALUES (ARRAY[20]);")
    run_sql("INSERT INTO users VALUES (ARRAY[2, 20, 200]);")

    disable_index_scan = "SET enable_indexscan = off"
    select = "SELECT * FROM users WHERE scores = ARRAY[2, 20, 200]"
    output = <<-OUTPUT
#{disable_index_scan};
EXPLAIN (COSTS OFF) #{select};
                       QUERY PLAN                       
--------------------------------------------------------
 Bitmap Heap Scan on users
   Recheck Cond: (scores = '{2,20,200}'::integer[])
   ->  Bitmap Index Scan on users_scores
         Index Cond: (scores = '{2,20,200}'::integer[])
(4 rows)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{disable_index_scan};\n" +
                                 "EXPLAIN (COSTS OFF) #{select};"))
    output = <<-OUTPUT
#{select};
   scores   
------------
 {2,20,200}
(1 row)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{select};"))
  end

  test "jsonb" do
    run_sql("CREATE TABLE memos (content jsonb);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    run_sql("INSERT INTO memos VALUES ('" + <<-JSON + "')");
{
  "string": "hello",
  "number": 1,
  "boolean": true,
  "array": [
    1,
    "hello",
    {
      "object": {
        "string": "world"
      }
    },
    false
  ]
}
    JSON

    disable_index_scan = "SET enable_indexscan = off"
    select = "SELECT jsonb_pretty(content) FROM memos WHERE content &@ 'Hello'"
    output = <<-OUTPUT
#{disable_index_scan};
EXPLAIN (COSTS OFF) #{select};
                   QUERY PLAN                   
------------------------------------------------
 Bitmap Heap Scan on memos
   Recheck Cond: (content &@ 'Hello'::text)
   ->  Bitmap Index Scan on memos_content
         Index Cond: (content &@ 'Hello'::text)
(4 rows)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{disable_index_scan};\n" +
                                 "EXPLAIN (COSTS OFF) #{select};"))
    output = <<-OUTPUT
#{select};
           jsonb_pretty            
-----------------------------------
 {                                +
     "array": [                   +
         1,                       +
         "hello",                 +
         {                        +
             "object": {          +
                 "string": "world"+
             }                    +
         },                       +
         false                    +
     ],                           +
     "number": 1,                 +
     "string": "hello",           +
     "boolean": true              +
 }
(1 row)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{select};"))
  end

  test "jsonb: sequential search" do
    run_sql("CREATE TABLE memos (content jsonb);")
    run_sql("INSERT INTO memos VALUES ('" + <<-JSON + "')");
{
  "string": "hello",
  "number": 1,
  "boolean": true,
  "array": [
    1,
    "hello",
    {
      "object": {
        "string": "world"
      }
    },
    false
  ]
}
    JSON

    select = "SELECT jsonb_pretty(content) FROM memos WHERE content &@ 'Hello'"
    output = <<-OUTPUT
EXPLAIN (COSTS OFF) #{select};
              QUERY PLAN              
--------------------------------------
 Seq Scan on memos
   Filter: (content &@ 'Hello'::text)
(2 rows)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("EXPLAIN (COSTS OFF) #{select};"))
    output = <<-OUTPUT
#{select};
           jsonb_pretty            
-----------------------------------
 {                                +
     "array": [                   +
         1,                       +
         "hello",                 +
         {                        +
             "object": {          +
                 "string": "world"+
             }                    +
         },                       +
         false                    +
     ],                           +
     "number": 1,                 +
     "string": "hello",           +
     "boolean": true              +
 }
(1 row)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{select};"))
  end

  test "name: Japanese" do
    run_sql("CREATE TABLE メモ (コンテンツ text);")
    run_sql("CREATE INDEX メモ_コンテンツ ON メモ USING pgroonga (コンテンツ);")
    run_sql("INSERT INTO メモ VALUES ('PGroonga is good!');")

    select = "SELECT * FROM メモ WHERE コンテンツ &@ 'PGroonga'"
    output = <<-OUTPUT
EXPLAIN (COSTS OFF) #{select};
                       QUERY PLAN                       
--------------------------------------------------------
 Bitmap Heap Scan on "メモ"
   Recheck Cond: ("コンテンツ" &@ 'PGroonga'::text)
   ->  Bitmap Index Scan on "メモ_コンテンツ"
         Index Cond: ("コンテンツ" &@ 'PGroonga'::text)
(4 rows)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("EXPLAIN (COSTS OFF) #{select};"))
    output = <<-OUTPUT
#{select};
    コンテンツ     
-------------------
 PGroonga is good!
(1 row)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{select};"))
  end

  test "options: tokenizer" do
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

  test "options: normalizer" do
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

  test "options: token filters" do
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

  test "options: plugins" do
    require_groonga_version(14, 0, 5)
    run_sql("CREATE TABLE memos (content text);")
    begin
      run_sql(<<-CREATE_INDEX)
CREATE INDEX memos_content ON memos
 USING pgroonga (content)
  WITH (plugins = 'token_filters/stem',
        token_filters = 'TokenFilterStem');
      CREATE_INDEX
    rescue Helpers::CommandRunError => error
      if error.error.include?("cannot find plugin file")
        omit("token_filters/stem plugin isn't available")
      end
      raise
    end
    run_sql("INSERT INTO memos VALUES ('It works');")
    run_sql("INSERT INTO memos VALUES ('I work');")
    run_sql("INSERT INTO memos VALUES ('I worked');")

    disable_index_scan = "SET enable_indexscan = off"
    select = "SELECT * FROM memos WHERE content &@ 'work'"
    output = <<-OUTPUT
#{disable_index_scan};
EXPLAIN (COSTS OFF) #{select};
                  QUERY PLAN                   
-----------------------------------------------
 Bitmap Heap Scan on memos
   Recheck Cond: (content &@ 'work'::text)
   ->  Bitmap Index Scan on memos_content
         Index Cond: (content &@ 'work'::text)
(4 rows)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{disable_index_scan};\n" +
                                 "EXPLAIN (COSTS OFF) #{select};"))

    output = <<-OUTPUT
#{select};
 content  
----------
 It works
 I work
 I worked
(3 rows)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{select};"))
  end

  test "delete" do
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")
    run_sql("INSERT INTO memos VALUES ('Groonga is good!');")

    select = "SELECT * FROM memos WHERE content &@ 'good'"
    output = <<-OUTPUT
#{select};
      content      
-------------------
 PGroonga is good!
 Groonga is good!
(2 rows)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{select};"))

    run_sql("DELETE FROM memos WHERE content &@ 'PGroonga';")
    run_sql("VACUUM memos;")
    run_sql("INSERT INTO memos VALUES ('Groonga is very good!');")
    select = <<-SELECT
SELECT *
  FROM
    pgroonga_result_to_recordset(
      pgroonga_command(
        'select',
        ARRAY[
          'command_version', '3',
          'query', 'content:@good',
          'table', pgroonga_table_name('memos_content')
        ]
      )::jsonb
    ) AS record(
      _id bigint,
      _key bigint,
      content text
    )
    SELECT
    output = <<-OUTPUT
#{select};
 _id | _key |        content        
-----+------+-----------------------
   1 |    1 | Groonga is very good!
   2 |    2 | Groonga is good!
(2 rows)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{select};"))
  end

  test "truncate: in transaction" do
    run_sql(<<-SQL)
BEGIN TRANSACTION;

CREATE TABLE memos (content text);

INSERT INTO memos VALUES ('PGroonga is good!');

CREATE INDEX memos_content ON memos USING pgroonga (content);

TRUNCATE memos;

INSERT INTO memos VALUES ('Groonga is good!');
INSERT INTO memos VALUES ('PGroonga is very good!');

COMMIT;
    SQL

    disable_index_scan = "SET enable_indexscan = off"
    select = "SELECT * FROM memos WHERE content &@ 'PGroonga'"
    output = <<-OUTPUT
#{disable_index_scan};
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
                 run_sql_standby("#{disable_index_scan};\n" +
                                 "EXPLAIN (COSTS OFF) #{select};"))

    output = <<-OUTPUT
#{select};
        content         
------------------------
 PGroonga is very good!
(1 row)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{select};"))
  end

  test "cache: insert" do
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")
    sleep(1) # To ensure caching the next pgroonga_command('select') result

    select = <<-SELECT.chomp
SELECT pgroonga_command(
         'select',
         ARRAY['table', pgroonga_table_name('memos_content')]
       )::jsonb->1->0->0->0
    SELECT

    run_sql_standby do |input, output, error|
      input.puts("#{select};")
      input.flush
      result = <<-RESULT
#{select};
 ?column? 
----------
 1
(1 row)

      RESULT
      assert_equal([result, ""],
                   [
                     output.read_command_output_all +
                     output.read_command_output_all(initial_timeout: 5),
                     error.read_command_output,
                   ])
      run_sql("INSERT INTO memos VALUES ('PGroonga is very good!');")

      input.puts("#{select};")
      input.flush
      result = <<-RESULT
#{select};
 ?column? 
----------
 2
(1 row)

      RESULT
      assert_equal([result, ""],
                   [
                     output.read_command_output_all +
                     output.read_command_output_all(initial_timeout: 5),
                     error.read_command_output,
                   ])
    end
  end

  test "cache: delete" do
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")
    sleep(1) # To ensure caching the next pgroonga_command('select') result

    select = <<-SELECT.chomp
SELECT pgroonga_command(
         'select',
         ARRAY['table', pgroonga_table_name('memos_content'), 'cache', 'no']
       )::jsonb->1->0->0->0
    SELECT

    run_sql_standby do |input, output, error|
      input.puts("#{select};")
      input.flush
      result = <<-RESULT
#{select};
 ?column? 
----------
 1
(1 row)

      RESULT
      assert_equal([result, ""],
                   [
                     output.read_command_output_all +
                     output.read_command_output_all(initial_timeout: 5),
                     error.read_command_output,
                   ])
      run_sql("DELETE FROM memos;")
      before = run_sql_standby("SELECT latest_end_lsn FROM pg_stat_wal_receiver;")
      # Use index scan to delete the deleted record from Groonga
      run_sql("SET enable_bitmapscan = no;",
              "SELECT * FROM memos WHERE content &@ 'pgroonga';")
      5.times do
        after = run_sql_standby("SELECT latest_end_lsn FROM pg_stat_wal_receiver;")
        break unless after == before
        sleep(1)
      end

      input.puts("#{select};")
      input.flush
      result = <<-RESULT
#{select};
 ?column? 
----------
 0
(1 row)

      RESULT
      assert_equal([result, ""],
                   [
                     output.read_command_output_all +
                     output.read_command_output_all(initial_timeout: 5),
                     error.read_command_output,
                   ])
    end
  end

  test "random crash" do
    require_pgroonga_benchmark

    revert_synchronous_replication

    dir = File.join(@tmp_dir, "pgroonga-benchmark")
    standby_recovering = false
    pgroonga_benchmark = Thread.new do
      ENV["WIKIPEDIA_INSERT_N_PAGES"] = "1000"
      ENV["PGHOST"] = @postgresql.host
      ENV["PGPORT"] = @postgresql.port.to_s
      ENV["PGDATABASE"] = @test_db_name
      ENV["PGUSER"] = @postgresql.user
      config = PGroongaBenchmark::Config.new(dir)
      config.use_builtin_benchmark("wikipedia-insert")
      status = PGroongaBenchmark::Status.new(dir)
      processor = PGroongaBenchmark::Processor.new(config, status)
      processor.process do |n_jobs, n_remained_jobs|
        Thread.stop if standby_recovering
      end
    end

    while pgroonga_benchmark.alive?
      output, error = run_sql_standby(<<-SELECT)
SELECT pid FROM pg_stat_activity WHERE backend_type = 'startup';
      SELECT
      pid = Integer(output[/\s+(\d+)$/, 1], 10)
      Process.kill(:KILL, pid)
      begin
        @postgresql_standby.stop
      rescue Helpers::CommandRunError
      end
      standby_recovering = true
      # @postgresql_standby.remove_logs
      @postgresql_standby.start
      standby_recovering = false
      begin
        pgroonga_benchmark.run
      rescue ThreadError
        # pgroonga_benchmark may be finished
      end
    end

    sleep(1)

    select = "SELECT id FROM pages WHERE content &@~ 'that';"
    select_result_standby = run_sql_standby(select)
    previous_select_result_standby = nil
    # wait for all WALs are applied on standby
    while select_result_standby != previous_select_result_standby
      previous_select_result_standby = select_result_standby
      sleep(1)
      select_result_standby = run_sql_standby(select)
    end
    assert_equal(run_sql(select), select_result_standby)
  end
end
