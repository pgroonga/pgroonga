require_relative "helpers/sandbox"

class PGroongaParameterTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

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
        assert_false(pgroonga_log.scan(/\|d\|.*pgroonga:/).empty?,
                      pgroonga_log)

        postgresql_log = @postgresql.read_log
        assert_true(postgresql_log.scan(/pgroonga:log:.*\|d\| pgroonga:/).empty?,
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
        assert_true(pgroonga_log.scan(/\|d\|.*pgroonga:/).empty?,
                      pgroonga_log)

        postgresql_log = @postgresql.read_log
        assert_false(postgresql_log.scan(/pgroonga:log:.*\|d\| pgroonga:/).empty?,
                    postgresql_log)
      end
    end
  end
end
