require "fileutils"
require "json"
require "socket"
require "stringio"
require "tempfile"

module Helpers
  class CommandRunError < StandardError
    attr_reader :commane_line
    attr_reader :output
    attr_reader :error
    def initialize(command_line, output, error)
      @command_line = command_line
      @output = output
      @error = error
      message = +"failed to run: "
      message << command_line.join(" ")
      message << "\n"
      message << "output:\n"
      message << output
      message << "error:\n"
      message << error
      super(message)
    end
  end

  module CommandOutputReadable
    def read_command_output_all(initial_timeout: 1)
      all_output = +""
      timeout = initial_timeout
      loop do
        break unless IO.select([self], nil, nil, timeout)
        all_output << read_command_output
        timeout = 0
      end
      all_output
    end

    def read_command_output
      return "" unless IO.select([self], nil, nil, 0)
      begin
        data = readpartial(4096).gsub(/\r\n/, "\n")
        data.force_encoding("UTF-8")
        data
      rescue EOFError
        ""
      end
    end
  end

  module CommandRunnable
    def spawn_process(*command_line)
      default_env = {
        "LC_ALL" => "C",
        "PGCLIENTENCODING" => "UTF-8",
      }
      IO.pipe do |input_read, input_write|
        input_write.sync = true
        IO.pipe do |output_read, output_write|
          IO.pipe do |error_read, error_write|
            output_read.extend(CommandOutputReadable)
            error_read.extend(CommandOutputReadable)
            options = {
              in: input_read,
              out: output_write,
              err: error_write,
            }
            if command_line[0].is_a?(Hash)
              command_line[0] = default_env.merge(command_line[0])
            else
              command_line.unshift(default_env)
            end
            pid = spawn(*command_line, options)
            begin
              input_read.close
              output_write.close
              error_write.close
              yield(pid, input_write, output_read, error_read)
            ensure
              finished = false
              begin
                finished = !Process.waitpid(pid, Process::WNOHANG).nil?
              rescue SystemCallError
                # Finished
              else
                unless finished
                  Process.kill(:KILL, pid)
                  Process.waitpid(pid)
                end
              end
            end
          end
        end
      end
    end

    def run_command(*command_line)
      spawn_process(*command_line) do |pid, input_write, output_read, error_read|
        output = +""
        error = +""
        status = nil
        timeout = 1
        if block_given?
          begin
            yield(input_write, output_read, output_read)
          ensure
            input_write.close unless input_write.closed?
          end
        end
        loop do
          readables, = IO.select([output_read, error_read], nil, nil, timeout)
          if readables
            timeout = 0
            readables.each do |readable|
              if readable == output_read
                output << output_read.read_command_output
              else
                error << error_read.read_command_output
              end
            end
          else
            timeout = 1
          end
          _, status = Process.waitpid2(pid, Process::WNOHANG)
          break if status
        end
        output << output_read.read_command_output
        error << error_read.read_command_output
        unless status.success?
          raise CommandRunError.new(command_line, output, error)
        end
        [output, error]
      end
    end

    def find_command(command)
      ENV["PATH"].split(File::PATH_SEPARATOR).each do |path|
        next unless File.absolute_path?(path)
        absolute_path_command = File.join(path, command)
        return absolute_path_command if File.executable?(absolute_path_command)
      end
      nil
    end
  end

  module PlatformDetectable
    private
    def windows?
      /mingw|mswin|cygwin/.match?(RUBY_PLATFORM)
    end
  end

  class PostgreSQL
    include CommandRunnable
    include PlatformDetectable

    attr_reader :dir
    attr_reader :host
    attr_reader :port
    attr_reader :user
    attr_reader :replication_user
    attr_reader :replication_password
    attr_reader :version
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
      @version = nil
      @pid = nil
      @running = false
    end

    def running?
      @running
    end

    def postgresql_conf
      File.join(@dir, "postgresql.conf")
    end

    def append_configuration(configuration)
      File.open(postgresql_conf, "a") do |conf|
        conf.puts(configuration)
      end
    end

    def initdb(db_path: "db",
               port: 15432,
               shared_preload_libraries: [])
      @dir = File.join(@base_dir, db_path)
      @log_path = File.join(@dir, "log", @log_base_name)
      @pgroonga_log_path = File.join(@dir, "pgroonga.log")
      socket_dir = File.join(@dir, "socket")
      @port = port
      @replication_user = "replicator"
      run_command("initdb",
                  "--locale", "C",
                  "--encoding", "UTF-8",
                  "--username", @user,
                  "-D", @dir)
      FileUtils.mkdir_p(socket_dir)
      conf = StringIO.new
      conf.puts("application_name = 'primary'")
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
      yield(conf) if block_given?
      append_configuration(conf.string)
      pg_hba_conf = File.join(@dir, "pg_hba.conf")
      File.open(pg_hba_conf, "a") do |conf|
        conf.puts("host replication #{@replication_user} #{@host}/32 trust")
      end
      @version = Integer(File.read(File.join(@dir, "PG_VERSION")).chomp, 10)
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
                  "--create-slot",
                  "--slot", "standby",
                  "--host", primary.host,
                  "--port", primary.port.to_s,
                  "--pgdata", @dir,
                  "--username", primary.replication_user,
                  "--write-recovery-conf",
                  "--verbose")
      conf = StringIO.new
      conf.puts("application_name = 'standby'")
      conf.puts("cluster_name = 'standby'")
      conf.puts("primary_slot_name = 'standby'")
      conf.puts("hot_standby = on")
      conf.puts("port = #{@port}")
      yield(conf) if block_given?
      append_configuration(conf.string)
      conf = File.read(postgresql_conf)
      conf = conf.gsub(/^shared_preload_libraries = '(.*?)'/) do
        libraries = $1.split(/\s*,\s*/) + shared_preload_libraries
        "shared_preload_libraries = '#{libraries.join(",")}'"
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
      pid_path = File.join(@dir, "postmaster.pid")
      if File.exist?(pid_path)
        first_line = File.readlines(pid_path, chomp: true)[0]
        begin
          @pid = Integer(first_line, 10)
        rescue ArgumentError
        end
      end
    end

    def stop
      return unless running?
      begin
        run_command("pg_ctl", "stop",
                    "-D", @dir)
      rescue CommandRunError => error
        if @pid
          begin
            Process.kill(:KILL, @pid)
          rescue SystemCallError
          end
          @pid = nil
          @running = false
        end
        error.message << "\nPostgreSQL log:\n#{read_log}"
        raise
      else
        @pid = nil
        @running = false
      end
    end

    def psql(db, *sqls, may_wait_crash_safer_preparing: false, &block)
      if may_wait_crash_safer_preparing
        begin
          return psql_internal(db, *sqls, &block)
        rescue Helpers::CommandRunError => error
          case error.error.chomp
          when "ERROR:  pgroonga: pgroonga_crash_safer is preparing"
            # This may be happen on slow environment
            sleep(3)
          else
            raise
          end
        end
      end
      psql_internal(db, *sqls, &block)
    end

    def groonga(*command_line)
      pgrn = Dir.glob("#{@dir}/base/*/pgrn").first
      output, _ = run_command("groonga",
                              pgrn,
                              *command_line)
      JSON.parse(output)
    end

    def read_log
      return "" unless File.exist?(@log_path)
      File.read(@log_path)
    end

    def read_pgroonga_log
      return "" unless File.exist?(@pgroonga_log_path)
      File.read(@pgroonga_log_path)
    end

    private
    def psql_internal(db, *sqls, &block)
      command_line = [
        "psql",
        "--host", @host,
        "--port", @port.to_s,
        "--username", @user,
        "--dbname", db,
        "--echo-all",
        "--no-psqlrc",
      ]
      sqls.each do |sql|
        command_line << "--command" << sql
      end
      output, error = run_command(*command_line, &block)
      output = normalize_output(output)
      [output, error]
    end

    def normalize_output(output)
      normalized_output = +""
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
    include PlatformDetectable

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

    def psql(db, *sqls, **options, &block)
      @postgresql.psql(db, *sqls, **options, &block)
    end

    def run_sql(*sqls, **options, &block)
      psql(@test_db_name, *sqls, **options, &block)
    end

    def psql_standby(db, *sqls, **options, &block)
      @postgresql_standby.psql(db, *sqls, **options, &block)
    end

    def run_sql_standby(*sqls, **options, &block)
      psql_standby(@test_db_name, *sqls, **options, &block)
    end

    def groonga(*command_line)
      @postgresql.groonga(*command_line)
    end

    def groonga_version
      version = groonga("status")[1]["version"]
      major_minor_micro, tag = version.split("-", 2)
      major, minor, micro = major_minor_micro.split(".").collect(&:to_i)
      [major, minor, micro, tag]
    end

    def require_groonga_version(required_major, required_minor, required_micro)
      major, minor, micro, tag = groonga_version
      compared = ([major, minor, micro] <=>
                  [required_major, required_minor, required_micro])
      return if compared > 0
      return if compared == 0 and tag.nil?
      if tag and [major, minor, micro + 1] ==
                 [required_major, required_minor, required_micro]
        # Unreleased version
        return
      end
      omit("Require Groonga " +
           "#{required_major}.#{required_minor}.#{required_micro}: " +
           "#{major}.#{minor}.#{micro}#{tag ? "-" : ""}#{tag}")

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
      <<-CONFIG
pgroonga.enable_wal = yes
      CONFIG
    end

    def teardown_db
    end

    def shared_preload_libraries_standby
      []
    end

    def setup_standby_db
      @postgresql_standby = nil
      omit("TODO: Support Windows") if windows?
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

    def start_postgres_standby
      @postgresql_standby.start
    end

    def stop_postgres_standby
      @postgresql_standby.stop
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
      <<-CONFIG
pgroonga.enable_wal = yes
      CONFIG
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
