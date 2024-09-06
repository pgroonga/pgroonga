require_relative "helpers/sandbox"

class PGroongaParameterTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  def scan_pgroonga_debug_log(log)
    log.scan(/\|d\|\d+: (.*)$/)
  end

  def scan_postgresql_debug_log(log)
    log.scan(/\|d\| (.*) \d+$/)
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
        assert_equal(["DDL:2147483654:set_normalizers NormalizerAuto"],
                     scan_pgroonga_debug_log(pgroonga_log).first,
                     pgroonga_log)
        postgresql_log = @postgresql.read_log
        assert_equal([],
                     scan_postgresql_debug_log(postgresql_log),
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
        assert_equal([],
                     scan_pgroonga_debug_log(pgroonga_log),
                     pgroonga_log)
        postgresql_log = @postgresql.read_log
        assert_equal(["DDL:2147483654:set_normalizers NormalizerAuto"],
                     scan_postgresql_debug_log(postgresql_log).first,
                     postgresql_log)
      end
    end
  end
end
