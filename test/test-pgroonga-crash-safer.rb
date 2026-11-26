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

  def wal_paths
    Dir.glob(File.join(@test_db_dir, "pgrn*.wal"))
  end

  def ensure_wal_flushed
    max_n_tries = flush_naptime * 60
    n_tries = 0
    until wal_paths.empty? do
      sleep(0.1)
      n_tries += 1
      if n_tries >= max_n_tries
        files = Dir.glob(File.join(@test_db_dir, "pgrn*"))
        raise "pgrn*.wal aren't flushed: #{files.join(", ")}"
      end
    end
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

  test "reset positions in non public schema" do
    run_sql("DROP EXTENSION pgroonga;")
    run_sql("CREATE SCHEMA test_schema;")
    run_sql("CREATE EXTENSION pgroonga WITH SCHEMA test_schema;")
    run_sql("CREATE TABLE test_schema.memos (title text);")
    run_sql("CREATE INDEX memos_title ON test_schema.memos USING pgroonga (title);")
    run_sql("INSERT INTO test_schema.memos VALUES ('PGroonga');")
    status_sql = "SELECT test_schema.pgroonga_wal_status();"
    status = run_sql(status_sql)
    run_sql("SELECT test_schema.pgroonga_wal_set_applied_position(100, 100);")
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
    ensure_wal_flushed
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
                 target_finish_log_lines,
                 @postgresql.read_pgroonga_log)
  end

  test "multiple connections to database without flush process" do
    omit("SIGSTOP isn't available on Windows") if windows?

    psql("postgres", "CREATE DATABASE test_new TEMPLATE #{@test_db_name};")
    main_pid_sql = <<-SQL
SELECT pid FROM pg_stat_activity
 WHERE backend_type = 'pgroonga: crash-safer: main';
    SQL
    main_pid = Integer(select_value("postgres", main_pid_sql), 10)
    # Multiple connections use the database before the main process
    # processes a request to start a flush process.
    Process.kill(:STOP, main_pid)
    begin
      threads = 2.times.collect do
        Thread.new do
          select_value("test_new",
                       "SET statement_timeout = '10s';",
                       "SELECT pgroonga_command('status')::jsonb->0->0;",
                       may_wait_crash_safer_preparing: true)
        end
      end
      sleep(1)
    ensure
      Process.kill(:CONT, main_pid)
    end
    assert_equal(["0", "0"], threads.collect(&:value))
  end

  sub_test_case("max_worker_processes") do
    def additional_configurations
      super + <<-CONFIG
max_worker_processes = 4
      CONFIG
    end

    test "too small" do
      error = nil
      8.times do |i|
        db = "test#{i}"
        psql("postgres", "CREATE DATABASE #{db} TEMPLATE #{@test_db_name};")
        begin
          psql(db,
               "SET statement_timeout = '10s';",
               "SELECT pgroonga_command('status');",
               may_wait_crash_safer_preparing: true)
        rescue Helpers::CommandRunError => error
          break
        end
      end
      assert_equal(<<-ERROR, error&.error)
ERROR:  pgroonga: pgroonga_crash_safer failed to start a flush process
HINT:  You may need to increase max_worker_processes.
      ERROR
    end
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

  sub_test_case("WAL resource manager") do
    setup def check_postgresql_version
      if @postgresql.version < 15
        omit("custom WAL resource manager is available since PostgreSQL 15")
      end
    end

    def shared_preload_libraries
      ["pgroonga_wal_resource_manager"] + super
    end

    def additional_configurations
      super + <<-CONFIG
pgroonga.enable_wal_resource_manager = yes
      CONFIG
    end

    # The test database is created after pgroonga_crash_safer is
    # started. pgroonga_crash_safer must start a flush process for it.
    test "flush database created after start" do
      run_sql("CREATE TABLE memos (content text);")
      run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
      run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")
      assert_not_equal([], wal_paths)
      ensure_wal_flushed
      assert_equal([], wal_paths)
    end

    sub_test_case("standby") do
      setup :setup_standby_db
      teardown :teardown_standby_db

      # pgroonga.enable_wal and pgroonga_wal_resource_manager can't be
      # used together on standby. Both of them apply the same changes.
      def additional_standby_configurations
        <<-CONFIG
pgroonga.enable_wal = no
        CONFIG
      end

      # Flush processes don't appear in pg_stat_activity.
      def flush_processes_standby
        postmaster_pid_path = File.join(@postgresql_standby.dir,
                                        "postmaster.pid")
        postmaster_pid = File.readlines(postmaster_pid_path, chomp: true)[0]
        ps = IO.popen(["ps", "-e", "-o", "ppid=,args="], &:read)
        ps.lines(chomp: true).grep(
          /\A\s*#{postmaster_pid}\s.*pgroonga: crash-safer: flush:/)
      end

      def setup_memos
        run_sql("CREATE TABLE memos (content text);")
        run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
        run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")
        @postgresql_standby.wait_replay(@postgresql.current_wal_lsn)
      end

      def count_standby
        @postgresql_standby.select_value(
          @test_db_name,
          "SET enable_seqscan = no;",
          "SELECT count(*) FROM memos WHERE content &@~ 'good';")
      end

      # The backend may be still running after psql is finished.
      def n_released_standby(pid)
        finalized_log_pattern = /\|#{pid}: pgroonga: \[exit\]\[finalize\]$/
        release_log_pattern =
          /\|#{pid}: pgroonga: \[exit\]\[finalize\]\[crash-safer\]\[release\]$/
        pgroonga_log = nil
        600.times do
          pgroonga_log = @postgresql_standby.read_pgroonga_log
          break if pgroonga_log.match?(finalized_log_pattern)
          sleep(0.1)
        end
        pgroonga_log.scan(release_log_pattern).size
      end

      # pgroonga_wal_resource_manager opens Groonga's database with
      # GRN_WAL_ROLE_PRIMARY in the startup process on
      # standby. pgroonga_crash_safer must not start flush processes
      # until the standby is promoted.
      test "flush after promotion" do
        setup_memos
        assert_equal(["1", []],
                     [count_standby, flush_processes_standby])
        run_sql_standby("SELECT pg_promote();")
        run_sql_standby("INSERT INTO memos VALUES ('Groonga is good!');",
                        may_wait_crash_safer_preparing: true)
        assert_equal(1, flush_processes_standby.size)
      end

      # A connection that is established during recovery is kept after
      # promotion. It must use pgroonga_crash_safer before it writes
      # something.
      test "connection over promotion" do
        setup_memos
        pid = nil
        run_sql_standby do |input, output, error|
          input.puts("\\pset tuples_only on")
          output.gets # \pset tuples_only on
          input.puts("SELECT pg_backend_pid();")
          output.gets # SELECT pg_backend_pid();
          pid = Integer(output.gets.strip, 10)
          # Open Groonga's database during recovery.
          input.puts("SELECT pgroonga_command('status');")
          input.puts("SELECT pg_promote();")
          output.gets("SELECT pg_promote();\n t\n")
          # Avoid the "pgroonga_crash_safer is preparing" error.
          run_sql_standby("SELECT pgroonga_command('status');",
                          may_wait_crash_safer_preparing: true)
          input.puts("INSERT INTO memos VALUES ('Groonga is good!');")
          input.close
          output.read
        end
        assert_equal(1, n_released_standby(pid))
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
