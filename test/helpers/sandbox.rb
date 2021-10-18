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
      loop do
        readables, = IO.select([output_read, error_read], nil, nil, 0)
        (readables || []).each do |readable|
          if readable == output_read
            output << read_command_output(output_read)
          else
            error << read_command_output(error_read)
          end
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

    def initdb
      @dir = File.join(@base_dir, "db")
      @log_path = File.join(@dir, "log", @log_base_name)
      socket_dir = File.join(@dir, "socket")
      @port = 15432
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
        conf.puts("shared_preload_libraries = 'pgroonga_check'")
        conf.puts("pgroonga.enable_wal = yes")
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

    def init_replication(primary)
      @dir = File.join(@base_dir, "db-standby")
      @log_path = File.join(@dir, "log", @log_base_name)
      @port = primary.port + 1
      run_command("pg_basebackup",
                  "--host", primary.host,
                  "--port", primary.port.to_s,
                  "--pgdata", @dir,
                  "--username", primary.replication_user,
                  "--write-recovery-conf")
      postgresql_conf = File.join(@dir, "postgresql.conf")
      File.open(postgresql_conf, "a") do |conf|
        conf.puts("hot_standby = on")
        conf.puts("port = #{@port}")
        yield(conf) if block_given?
      end
      conf = File.read(postgresql_conf)
      conf = conf.gsub(/^shared_preload_libraries = '(.*?)'/) do
        "shared_preload_libraries = '#{$1},pgroonga_wal_applier'"
      end
      File.write(postgresql_conf, conf)
    end

    def start
      begin
        run_command("pg_ctl", "start",
                    "-w",
                    "-D", @dir)
      rescue => error
        error.message << "\nPostgreSQL log:\n#{File.read(@log_path)}"
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
      run_command("psql",
                  "--host", @host,
                  "--port", @port.to_s,
                  "--username", @user,
                  "--dbname", db,
                  "--echo-all",
                  "--no-psqlrc",
                  "--command", sql)
    end

    def groonga(*command_line)
      pgrn = Dir.glob("#{@dir}/base/*/pgrn").first
      output, _ = run_command("groonga",
                              pgrn,
                              *command_line)
      JSON.parse(output)
    end

    private
    def windows?
      /mingw|mswin|cygwin/.match?(RUBY_PLATFORM)
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
      FileUtils.rm_rf(@tmp_dir)
    end

    def setup_db
      @postgresql = PostgreSQL.new(@tmp_dir)
      @postgresql.initdb
    end

    def teardown_db
    end

    def setup_standby_db
      @postgresql_standby = PostgreSQL.new(@tmp_dir)
      @postgresql_standby.init_replication(@postgresql) do |conf|
        conf.puts(additional_standby_configurations)
      end
      @postgresql_standby.start
    end

    def additional_standby_configurations
      ""
    end

    def teardown_standby_db
      @postgresql_standby.stop
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
      stop_postgres
    end

    def setup_test_db
      @test_db_name = "test"
      psql("postgres", "CREATE DATABASE #{@test_db_name}")
      run_sql("CREATE EXTENSION pgroonga")
      Dir.glob(File.join(@postgresql.dir, "base", "*", "pgrn")) do |pgrn|
        @test_db_dir = File.dirname(pgrn)
      end
    end

    def teardown_test_db
    end
  end
end
