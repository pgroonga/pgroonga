require "fileutils"
require "json"
require "socket"
require "stringio"
require "tempfile"

module Helpers
  module CommandRunnable
    def spawn_process(*args)
      env = {
        "LC_ALL" => "C",
        "PGCLIENTENCODING" => "UTF-8",
      }
      output_read, output_write = IO.pipe
      error_read, error_write = IO.pipe
      options = {
        :out => output_write,
        :err => error_write,
      }
      pid = spawn(env, *args, options)
      output_write.close
      error_write.close
      [pid, output_read, error_read]
    end

    def read_command_output(input)
      return "" unless IO.select([input], nil, nil, 0)
      begin
        data = input.readpartial(4096).gsub(/\r\n/, "\n")
        data.force_encoding("UTF-8")
        data
      rescue EOFError
        ""
      end
    end

    def run_command(*args)
      pid, output_read, error_read = spawn_process(*args)
      output = ""
      error = ""
      status = nil
      timeout = 1
      loop do
        readables, = IO.select([output_read, error_read], nil, nil, timeout)
        if readables
          timeout = 0
          readables.each do |readable|
            if readable == output_read
              output << read_command_output(output_read)
            else
              error << read_command_output(error_read)
            end
          end
        else
          timeout = 1
        end
        _, status = Process.waitpid2(pid, Process::WNOHANG)
        break if status
      end
      output << read_command_output(output_read)
      error << read_command_output(error_read)
      unless status.success?
        command_line = args.join(" ")
        message = "failed to run: #{command_line}\n"
        message << "output:\n"
        message << output
        message << "error:\n"
        message << error
        raise message
      end
      [output, error]
    end
  end

  class PostgreSQL
    include CommandRunnable

    attr_reader :dir
    attr_reader :host
    attr_reader :port
    attr_reader :user
    attr_reader :replication_user
    attr_reader :replication_password
    def initialize(base_dir)
      @base_dir = base_dir
      @dir = nil
      @log_base_name = "postgresql.log"
      @log_path = nil
      @host = "127.0.0.1"
      @port = nil
      @user = "pgroonga-test"
      @replication_user = nil
      @replication_password = nil
      @running = false
    end

    def running?
      @running
    end

    def initdb(shared_preload_libraries: [],
               db_path: "db",
               port: 15432)
      @dir = File.join(@base_dir, db_path)
      @log_path = File.join(@dir, "log", @log_base_name)
      socket_dir = File.join(@dir, "socket")
      @port = port
      @replication_user = "replicator"
      run_command("initdb",
                  "--locale", "C",
                  "--encoding", "UTF-8",
                  "--username", @user,
                  "-D", @dir)
      FileUtils.mkdir_p(socket_dir)
      postgresql_conf = File.join(@dir, "postgresql.conf")
      File.open(postgresql_conf, "a") do |conf|
        conf.puts("listen_addresses = '#{@host}'")
        conf.puts("port = #{@port}")
        unless windows?
          conf.puts("unix_socket_directories = '#{socket_dir}'")
        end
        conf.puts("logging_collector = on")
        conf.puts("log_filename = '#{@log_base_name}'")
        conf.puts("wal_level = replica")
        conf.puts("max_wal_senders = 4")
        conf.puts("shared_preload_libraries = " +
                  "'#{shared_preload_libraries.join(",")}'")
        conf.puts("pgroonga.enable_wal = yes")
        yield(conf) if block_given?
      end
      pg_hba_conf = File.join(@dir, "pg_hba.conf")
      File.open(pg_hba_conf, "a") do |conf|
        conf.puts("host replication #{@replication_user} #{@host}/32 trust")
      end
    end

    def create_replication_user
      run_command("createuser",
                  "--host", @host,
                  "--port", @port.to_s,
                  "--username", @user,
                  "--replication",
                  @replication_user)
    end

    def init_replication(primary, shared_preload_libraries: [])
      @dir = File.join(@base_dir, "db-standby")
      @log_path = File.join(@dir, "log", @log_base_name)
      @port = primary.port + 1
      run_command("pg_basebackup",
                  "--host", primary.host,
                  "--port", primary.port.to_s,
                  "--pgdata", @dir,
                  "--username", primary.replication_user,
                  "--write-recovery-conf",
                  "--verbose")
      postgresql_conf = File.join(@dir, "postgresql.conf")
      File.open(postgresql_conf, "a") do |conf|
        conf.puts("hot_standby = on")
        conf.puts("port = #{@port}")
        yield(conf) if block_given?
      end
      conf = File.read(postgresql_conf)
      conf = conf.gsub(/^shared_preload_libraries = '(.*?)'/) do
        libraries = [$1, *shared_preload_libraries].join(",")
        "shared_preload_libraries = '#{libraries}'"
      end
      File.write(postgresql_conf, conf)
    end

    def start
      begin
        run_command("pg_ctl", "start",
                    "-w",
                    "-D", @dir)
      rescue => error
        error.message << "\nPostgreSQL log:\n#{read_log}"
        raise
      end
      loop do
        begin
          TCPSocket.open(@host, @port) do
          end
        rescue SystemCallError
          sleep(0.1)
        else
          break
        end
      end
      @running = true
    end

    def stop
      return unless running?
      run_command("pg_ctl", "stop",
                  "-D", @dir)
    end

    def psql(db, sql)
      output, error = run_command("psql",
                                  "--host", @host,
                                  "--port", @port.to_s,
                                  "--username", @user,
                                  "--dbname", db,
                                  "--echo-all",
                                  "--no-psqlrc",
                                  "--command", sql)
      output = normalize_output(output)
      [output, error]
    end

    def groonga(*command_line)
      pgrn = Dir.glob("#{@dir}/base/*/pgrn").first
      output, _ = run_command("groonga",
                              pgrn,
                              *command_line)
      JSON.parse(output)
    end

    def read_log
      File.read(@log_path)
    end

    private
    def windows?
      /mingw|mswin|cygwin/.match?(RUBY_PLATFORM)
    end

    def normalize_output(output)
      normalized_output = ""
      output.each_line do |line|
        case line.chomp
        when "SET", "CREATE EXTENSION"
          next
        end
        normalized_output << line
      end
      normalized_output
    end
  end

  module Sandbox
    include CommandRunnable

    class << self
      def included(base)
        base.module_eval do
          setup :setup_tmp_dir
          teardown :teardown_tmp_dir

          setup :setup_db
          teardown :teardown_db

          setup :setup_postgres
          teardown :teardown_postgres

          setup :setup_test_db
          teardown :teardown_test_db
        end
      end
    end

    def psql(db, sql)
      @postgresql.psql(db, sql)
    end

    def run_sql(sql)
      psql(@test_db_name, sql)
    end

    def psql_standby(db, sql)
      @postgresql_standby.psql(db, sql)
    end

    def run_sql_standby(sql)
      psql_standby(@test_db_name, sql)
    end

    def groonga(*command_line)
      @postgresql.groonga(*command_line)
    end

    def setup_tmp_dir
      memory_fs = "/dev/shm"
      if File.exist?(memory_fs)
        @tmp_dir = File.join(memory_fs, "pgroonga")
      else
        @tmp_dir = File.join(__dir__, "tmp")
      end
      FileUtils.rm_rf(@tmp_dir)
      FileUtils.mkdir_p(@tmp_dir)
    end

    def teardown_tmp_dir
      debug_dir = ENV["PGROONGA_TEST_DEBUG_DIR"]
      if debug_dir and File.exist?(@tmp_dir)
        FileUtils.rm_rf(debug_dir)
        FileUtils.mv(@tmp_dir, debug_dir)
      else
        FileUtils.rm_rf(@tmp_dir)
      end
    end

    def setup_db
      @postgresql = PostgreSQL.new(@tmp_dir)
      options = {
        shared_preload_libraries: shared_preload_libraries,
      }
      @postgresql.initdb(**options) do |conf|
        conf.puts(additional_configurations)
      end
    end

    def shared_preload_libraries
      ["pgroonga_check"]
    end

    def additional_configurations
      ""
    end

    def teardown_db
    end

    def shared_preload_libraries_standby
      []
    end

    def setup_standby_db
      @postgresql_standby = PostgreSQL.new(@tmp_dir)
      options = {
        shared_preload_libraries: shared_preload_libraries_standby,
      }
      @postgresql_standby.init_replication(@postgresql, **options) do |conf|
        conf.puts(additional_standby_configurations)
      end
      @postgresql_standby.start
    end

    def additional_standby_configurations
      ""
    end

    def teardown_standby_db
      @postgresql_standby.stop if @postgresql_standby
    end

    def setup_reference_db
      @postgresql_reference = PostgreSQL.new(@tmp_dir)
      options = {
        db_path: "db-reference",
        port: 25432,
        shared_preload_libraries: reference_shared_preload_libraries,
      }
      @postgresql_reference.initdb(**options) do |conf|
        conf.puts(additional_reference_configurations)
      end
      @postgresql_reference.start
    end

    def reference_shared_preload_libraries
      []
    end

    def additional_reference_configurations
      ""
    end

    def teardown_reference_db
      @postgresql_reference.stop if @postgresql_reference
    end

    def start_postgres
      @postgresql.start
    end

    def stop_postgres
      @postgresql.stop
    end

    def setup_postgres
      start_postgres
      @postgresql.create_replication_user
    end

    def teardown_postgres
      stop_postgres if @postgresql
    end

    def create_db(postgresql, db_name)
      postgresql.psql("postgres", "CREATE DATABASE #{db_name}")
      postgresql.psql(db_name, "CREATE EXTENSION pgroonga")
      postgresql.psql(db_name, "CHECKPOINT")
    end

    def setup_test_db
      @test_db_name = "test"
      create_db(@postgresql, @test_db_name)
      result, = run_sql("SELECT oid FROM pg_catalog.pg_database " +
                        "WHERE datname = current_database()")
      oid = result.lines[3].strip
      @test_db_dir = File.join(@postgresql.dir, "base", oid)
    end

    def teardown_test_db
    end

    def setup_reference_test_db
      create_db(@postgresql_reference, @test_db_name)
    end

    def teardown_reference_test_db
    end
  end
end
