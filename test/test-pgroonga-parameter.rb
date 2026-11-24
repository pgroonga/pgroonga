require_relative "helpers/sandbox"

class PGroongaParameterTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  def scan_pgroonga_debug_log(log)
    log.scan(/\|d\|\d+: (.*)$/)
  end

  def scan_postgresql_debug_log(log)
    log.scan(/\|d\| (.*) \d+$/)
  end

  # Groonga on Windows reports "[io][close] <...>" as debug logs. We
  # remove them because they aren't reported on other platforms.
  def remove_io_close_logs(logs)
    logs.reject {|message,| message.start_with?("[io][close] <")}
  end

  sub_test_case "pgroonga.log_level = debug" do
    def additional_configurations
      <<-CONFIG
pgroonga.log_level = debug
      CONFIG
    end

    sub_test_case "pgroonga.log_type = file" do
      def additional_configurations
        super + <<-CONFIG
pgroonga.log_type = file
        CONFIG
      end

      test "log output" do
        pgroonga_log = @postgresql.read_pgroonga_log
        debug_logs = scan_pgroonga_debug_log(pgroonga_log)
        assert_equal(["DDL:2147483654:set_normalizers NormalizerAuto"],
                     remove_io_close_logs(debug_logs).first,
                     pgroonga_log)
        postgresql_log = @postgresql.read_log
        debug_logs = scan_postgresql_debug_log(postgresql_log)
        assert_equal([],
                     remove_io_close_logs(debug_logs),
                     postgresql_log)
      end

      test "override by query" do
        require_groonga_version(14, 0, 8)

        run_sql do |input, output, error|
          input.puts("SET pgroonga.log_type = postgresql;")
          input.puts("SELECT pgroonga_command('status');")
          input.close
        end

        postgresql_log = @postgresql.read_log
        debug_logs = scan_postgresql_debug_log(postgresql_log)
        assert_equal(["DDL:2147483654:set_normalizers NormalizerAuto"],
                     remove_io_close_logs(debug_logs).first,
                     postgresql_log)
      end
    end

    sub_test_case "pgroonga.log_type = postgresql" do
      def additional_configurations
        super + <<-CONFIG
pgroonga.log_type = postgresql
        CONFIG
      end

      test "log output" do
        pgroonga_log = @postgresql.read_pgroonga_log
        debug_logs = scan_pgroonga_debug_log(pgroonga_log)
        assert_equal([],
                     remove_io_close_logs(debug_logs),
                     pgroonga_log)
        postgresql_log = @postgresql.read_log
        debug_logs = scan_postgresql_debug_log(postgresql_log)
        assert_equal(["DDL:2147483654:set_normalizers NormalizerAuto"],
                     remove_io_close_logs(debug_logs).first,
                     postgresql_log)
      end

      test "override by query" do
        require_groonga_version(14, 0, 8)

        run_sql do |input, output, error|
          input.puts("SET pgroonga.log_type = file;")
          input.puts("SELECT pgroonga_command('status');")
          input.close
        end
        pgroonga_log = @postgresql.read_pgroonga_log
        debug_logs = scan_pgroonga_debug_log(pgroonga_log)
        assert_equal(["DDL:2147483654:set_normalizers NormalizerAuto"],
                     remove_io_close_logs(debug_logs).first,
                     pgroonga_log)
      end
    end
  end

  sub_test_case "pgroonga.log_rotate_threshold_size" do
    setup do
      require_groonga_version(14, 0, 7)
      stop_postgres
      @postgresql.append_configuration(<<-CONFIG)
pgroonga.log_rotate_threshold_size = 10
      CONFIG
      start_postgres
    end

    test "rotated" do
      run_sql("SELECT pgroonga_command('status');")
      assert do
        not Pathname.glob("#{@postgresql.dir}/pgroonga.log.*").empty?
      end
    end
  end

  sub_test_case "pgroonga.query_log_rotate_threshold_size" do
    setup do
      require_groonga_version(14, 0, 7)
      stop_postgres
      @postgresql.append_configuration(<<-CONFIG)
pgroonga.query_log_rotate_threshold_size = 10
pgroonga.query_log_path = 'pgroonga.query.log'
      CONFIG
      start_postgres
    end

    test "rotated" do
      run_sql("SELECT pgroonga_command('status');")
      assert do
        not Pathname.glob("#{@postgresql.dir}/pgroonga.query.log.*").empty?
      end
    end
  end
end
