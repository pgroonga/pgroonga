require_relative "helpers/fixture"
require_relative "helpers/sandbox"

class PGroongaCrashSaferTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  def shared_preload_libraries
    ["pgroonga_crash_safer"]
  end

  def flush_naptime
    1
  end

  def additional_configurations
    <<-CONFIG
pgroonga.enable_crash_safe = yes
pgroonga.enable_wal = yes
pgroonga.log_level = debug
pgroonga_crash_safer.flush_naptime = #{flush_naptime}
pgroonga_crash_safer.log_level = debug
    CONFIG
  end

  test "reset positions on primary" do
    run_sql("CREATE TABLE memos (title text, content text);")
    run_sql("CREATE INDEX memos_title ON memos USING pgroonga (title);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    run_sql("INSERT INTO memos VALUES ('PGroonga', 'PGroonga is good!');")
    status_sql = "SELECT pgroonga_wal_status();"
    status = run_sql(status_sql)
    run_sql("SELECT pgroonga_wal_set_applied_position(100, 100);")
    assert_not_equal(status, run_sql(status_sql))
    stop_postgres
    start_postgres
    assert_equal(status,
                 run_sql(status_sql, may_wait_crash_safer_preparing: true))
  end

  sub_test_case "standby" do
    setup :setup_standby_db
    teardown :teardown_standby_db

    test "not reset positions" do
      run_sql("CREATE TABLE memos (title text, content text);")
      run_sql("CREATE INDEX memos_title ON memos USING pgroonga (title);")
      run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
      run_sql("INSERT INTO memos VALUES ('PGroonga', 'PGroonga is good!');")
      status_sql = "SELECT pgroonga_wal_status();"
      original_status = run_sql_standby(status_sql)
      run_sql_standby("SELECT pgroonga_wal_set_applied_position(100, 100);")
      changed_status = run_sql_standby(status_sql)
      assert_not_equal(changed_status, original_status)
      stop_postgres_standby
      start_postgres_standby
      assert_equal(changed_status,
                   run_sql_standby(status_sql, may_wait_crash_safer_preparing: true))
    end
  end

  test "recover from WAL" do
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    max_n_tries = flush_naptime * 60
    n_tries = 0
    until Dir.glob(File.join(@test_db_dir, "pgrn*.wal")).empty? do
      sleep(0.1)
      n_tries += 1
      if n_tries >= max_n_tries
        files = Dir.glob(File.join(@test_db_dir, "pgrn*"))
        raise "pgrn*.wal aren't flushed: #{files.join(", ")}"
      end
    end
    Dir.glob(File.join(@test_db_dir, "pgrn*")) do |path|
      FileUtils.cp(path, "#{path}.bak")
    end
    run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")
    Dir.glob(File.join(@test_db_dir, "pgrn*.wal")) do |path|
      FileUtils.cp(path, "#{path}.bak")
    end
    stop_postgres
    Dir.glob(File.join(@test_db_dir, "pgrn*.bak")) do |path|
      FileUtils.cp(path, path.chomp(".bak"))
    end
    start_postgres
    sql = <<-SQL
SET enable_seqscan = no;
SELECT * FROM memos WHERE content &@~ 'PGroonga';
    SQL
    assert_equal([<<-OUTPUT, ""], run_sql(sql, may_wait_crash_safer_preparing: true))
#{sql}
      content      
-------------------
 PGroonga is good!
(1 row)

    OUTPUT
  end

  test "recover by REINDEX" do
    run_sql("CREATE TABLE memos (title text, content text);")
    run_sql("CREATE INDEX memos_title ON memos USING pgroonga (title);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    run_sql("INSERT INTO memos VALUES ('PGroonga', 'PGroonga is good!');")
    run_sql("SELECT pgroonga_wal_truncate();")
    stop_postgres
    File.open(File.join(@test_db_dir, "pgrn"), "w") do |pgrn|
      pgrn.puts("Broken")
    end
    start_postgres
    sql = <<-SQL
SET enable_seqscan = no;
SELECT * FROM memos WHERE content &@~ 'PGroonga';
    SQL
    assert_equal([<<-OUTPUT, ""], run_sql(sql, may_wait_crash_safer_preparing: true))
#{sql}
  title   |      content      
----------+-------------------
 PGroonga | PGroonga is good!
(1 row)

    OUTPUT
  end

  test "ensure no .wal after normal shutdown" do
    run_sql("CREATE TABLE memos (title text, content text);")
    run_sql("CREATE INDEX memos_title ON memos USING pgroonga (title);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    run_sql("INSERT INTO memos VALUES ('PGroonga', 'PGroonga is good!');")
    insert_connection_pid = nil
    begin
      run_sql do |input, output, error|
        input.puts("\\pset tuples_only on")
        output.gets # \pset tuples_only on
        input.puts("SELECT pg_backend_pid();")
        output.gets # SELECT pg_backend_pid();
        insert_connection_pid = Integer(output.gets.strip, 10)
        # Shutdown while INSERT-ing.
        input.puts(<<-INSERT)
INSERT INTO memos
  SELECT md5(clock_timestamp()::text),
         md5(clock_timestamp()::text)
    FROM generate_series(1, 10000);
        INSERT
        sleep(1)
        stop_postgres
        input.close
        output.read
      end
    rescue Helpers::CommandRunError
      # Ignore "terminating connection due to administrator command" error.
      # It's happen because shutdown is executed while INSERT-ing.
    end
    assert_equal([],
                 Dir.glob("#{@test_db_dir}/pgrn.*.wal"))
    crash_safer_pid = nil
    crash_safer_flushing_log_line = nil
    insert_connection_release_log_line = nil
    target_finish_log_lines = []
    @postgresql.read_pgroonga_log.each_line(chomp: true) do |line|
      case line
      when /\|(\d+): pgroonga: crash-safer: initialize/
        crash_safer_pid = Integer($1, 10)
      when /\|(\d+): pgroonga: crash-safer: flushing database before closing/
        if Integer($1, 10) == crash_safer_pid
          crash_safer_flushing_log_line = line
          target_finish_log_lines << line
        end
      when /\|(\d+): pgroonga: \[exit\]\[finalize\]\[crash-safer\]\[release\]/
        if Integer($1, 10) == insert_connection_pid
          insert_connection_release_log_line = line
          target_finish_log_lines << line
        end
      end
    end
    assert_equal([
                   insert_connection_release_log_line,
                   crash_safer_flushing_log_line,
                 ],
                 target_finish_log_lines)
  end

  sub_test_case("random crash") do
    include Helpers::Fixture

    setup :require_pgroonga_benchmark

    setup def omit_on_ci
      if ENV["CI"] == "true"
        omit("This test may take 2-60 min.") if data[:crash_ratio] > 0.1
      end
    end

    def additional_configurations
      super + <<-CONFIG
autovacuum = no
log_autovacuum_min_duration = 0
      CONFIG
    end

    def additional_reference_configurations
      super + <<-CONFIG
autovacuum = no
log_autovacuum_min_duration = 0
      CONFIG
    end

    setup :setup_reference_db
    teardown :teardown_reference_db

    setup :setup_reference_test_db
    teardown :teardown_reference_test_db

    setup def check_groonga_version
      base, tag = groonga("status")[1]["version"].split("-", 2)
      base = Gem::Version.new(base)
      if base >= Gem::Version.new("12.0.2")
        return
      end
      if base == Gem::Version.new("12.0.1") and tag
        return
      end
      omit("Groonga 12.0.2 or later is required")
    end

    data(:scenario, [
           "text-array-add",
           "text-array-update",
         ])
    data(:crash_ratio, [0.1, 0.5, 1.0])
    test "scenario" do
      dir = File.join(@tmp_dir, "pgroonga-benchmark")
      FileUtils.cp_r(fixture_path("crash-safer", data[:scenario]),
                     dir)
      File.open(File.join(dir, "config.yaml"), "w") do |output|
        config = {
          "test_crash_safe" => true,
          "crash_ratio" => data[:crash_ratio],
          "postgresql" => {
            "host" => @postgresql.host,
            "port" => @postgresql.port,
            "user" => @postgresql.user,
            "database" => @test_db_name,
          },
          "reference_postgresql" => {
            "host" => @postgresql_reference.host,
            "port" => @postgresql_reference.port,
            "user" => @postgresql_reference.user,
            "database" => @test_db_name,
          },
        }
        output.puts(config.to_yaml)
      end
      config = PGroongaBenchmark::Config.new(dir)
      status = PGroongaBenchmark::Status.new(dir)
      processor = PGroongaBenchmark::Processor.new(config, status)
      begin
        processor.process
      rescue PGroongaBenchmark::VerifyError => error
        if error.index_column_name
          assert_equal([0, []],
                       [
                         error.index_column_diff[0][0],
                         error.index_column_diff[1],
                       ],
                       "#{error.message}: #{error.index_column_name}")
        else
          assert_equal(error.expected_dumps[0],
                       error.actual_dumps[0],
                       error.message)
        end
      end
    end
  end

  test "max_recovery_threads: default" do
    pgroonga_log = @postgresql.read_pgroonga_log
    assert_equal(["pgroonga: crash-safer: max_recovery_threads: 0"],
                 pgroonga_log.scan(/pgroonga: crash-safer: max_recovery_threads:.*$/),
                 pgroonga_log)
  end

  sub_test_case("max_recovery_threads") do
    def additional_configurations
      super + <<-CONFIG
pgroonga_crash_safer.max_recovery_threads = -1
      CONFIG
    end

    test "set" do
      pgroonga_log = @postgresql.read_pgroonga_log
      assert_equal(["pgroonga: crash-safer: max_recovery_threads: -1"],
                   pgroonga_log.scan(/pgroonga: crash-safer: max_recovery_threads:.*$/),
                   pgroonga_log)
    end
  end
end
