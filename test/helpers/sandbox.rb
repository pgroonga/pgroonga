require "fileutils"
require "socket"
require "stringio"

module Helpers
  module Sandbox
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

    def spawn_process(*args)
      env = {}
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

    def run_command(*args)
      pid, output_read, error_read = spawn_process(*args)
      _, status = Process.waitpid2(pid)
      output = output_read.read
      error = error_read.read
      unless status.success?
        command_line = args.join(" ")
        raise "failed to run: #{command_line}: #{error}"
      end
      [output, error]
    end

    def psql(db, sql)
      run_command("psql",
                  "--host", @host,
                  "--port", @port.to_s,
                  "--dbname", db,
                  "--echo-all",
                  "--command", sql)
    end

    def run_sql(sql)
      psql(@test_db_name, sql)
    end

    def start_postgres
      @postgres_pid, @postgres_output, @postgres_error =
        spawn_process("postgres",
                      "-D", @db_dir)
      loop do
        begin
          TCPSocket.open(@host, @port) do
          end
        rescue SystemCallError
          pid = Process.waitpid(@postgres_pid, Process::WNOHANG)
          if pid
            message = "failed to start postgres:\n"
            message << "output:\n"
            message << @postgres_output.read
            message << "error:\n"
            message << @postgres_error.read
            @postgres_pid = nil
            raise message
          end
          sleep(0.1)
        else
          break
        end
      end
    end

    def stop_postgres
      return if @postgres_pid.nil?
      Process.kill(:TERM, @postgres_pid)
      _, status = Process.waitpid2(@postgres_pid)
      unless status.success?
        puts("failed to stop postgres:")
        puts("output:")
        puts(@postgres_output.read)
        puts("error:")
        puts(@postgres_error.read)
      end
    end

    def setup_tmp_dir
      memory_fs = "/dev/shm"
      if File.exist?(memory_fs)
        @tmp_dir = File.join(memory_fs, "pgroonga-check")
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
      @db_dir = @tmp_dir
      @socket_dir = File.join(@db_dir, "socket")
      @host = "127.0.0.1"
      @port = 15432
      run_command("initdb",
                  "--locale", "C",
                  "--encoding", "UTF-8",
                  "-D", @db_dir)
      FileUtils.mkdir_p(@socket_dir)
      postgresql_conf = File.join(@db_dir, "postgresql.conf")
      File.open(postgresql_conf, "a") do |conf|
        conf.puts("listen_addresses = '#{@host}'")
        conf.puts("port = #{@port}")
        conf.puts("unix_socket_directories = '#{@socket_dir}'")
        conf.puts("shared_preload_libraries = 'pgroonga-check.so'")
        conf.puts("pgroonga.enable_wal = yes")
      end
    end

    def teardown_db
    end

    def setup_postgres
      start_postgres
    end

    def teardown_postgres
      stop_postgres
    end

    def setup_test_db
      @test_db_name = "test"
      psql("postgres", "CREATE DATABASE #{@test_db_name}")
      run_sql("CREATE EXTENSION pgroonga")
      Dir.glob(File.join(@db_dir, "base", "*", "pgrn")) do |pgrn|
        @test_db_dir = File.dirname(pgrn)
      end
    end

    def teardown_test_db
    end
  end
end
