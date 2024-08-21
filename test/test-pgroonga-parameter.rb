require_relative "helpers/sandbox"

class PGroongaParameterTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  setup do
    @debug_log_pattern = {
      pgroonga_log: /\|d\|\d+: (.*)$/,
      postgresql_log: /\|d\| (.*) \d+$/,
    }
  end

  sub_test_case "pgroonga.log_type = file" do
    sub_test_case "pgroonga.log_level = debug" do
      def additional_configurations
        <<-CONFIG
pgroonga.log_type = file
pgroonga.log_level = debug
        CONFIG
      end

      test "log output" do
        pgroonga_log = @postgresql.read_pgroonga_log
        assert_equal(["DDL:2147483654:set_normalizers NormalizerAuto"],
                     pgroonga_log.scan(@debug_log_pattern[:pgroonga_log]).first,
                     pgroonga_log)

        postgresql_log = @postgresql.read_log
        assert_equal([],
                     postgresql_log.scan(@debug_log_pattern[:postgresql_log]),
                     postgresql_log)
      end
    end
  end

  sub_test_case "pgroonga.log_type = postgresql" do
    sub_test_case "pgroonga.log_level = debug" do
      def additional_configurations
        <<-CONFIG
pgroonga.log_type = postgresql
pgroonga.log_level = debug
        CONFIG
      end

      test "log output" do
        pgroonga_log = @postgresql.read_pgroonga_log
        assert_equal([],
                     pgroonga_log.scan(@debug_log_pattern[:pgroonga_log]),
                     pgroonga_log)

        postgresql_log = @postgresql.read_log
        assert_equal(["DDL:2147483654:set_normalizers NormalizerAuto"],
                     postgresql_log.scan(@debug_log_pattern[:postgresql_log]).first,
                     postgresql_log)
      end
    end
  end
end
