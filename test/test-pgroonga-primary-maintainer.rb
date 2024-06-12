require_relative "helpers/sandbox"

class PGroongaPrimaryMaintainerTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  def shared_preload_libraries
    ["pgroonga_primary_maintainer"]
  end

  sub_test_case "parameter" do
    def additional_configurations
      <<-CONFIG
pgroonga_primary_maintainer.naptime = 1
pgroonga_primary_maintainer.reindex_threshold = 512MB
      CONFIG
    end

    test "naptime" do
      postgresql_log = @postgresql.read_log
      assert_equal(["pgroonga: primary-maintainer: naptime=1"],
                   postgresql_log.scan(/pgroonga: primary-maintainer: naptime=.*$/),
                   postgresql_log)
    end

    test "reindex_threshold" do
      postgresql_log = @postgresql.read_log
      assert_equal(["pgroonga: primary-maintainer: reindex_threshold=65536"],
                   postgresql_log.scan(/pgroonga: primary-maintainer: reindex_threshold=.*$/),
                   postgresql_log)
    end
  end

  sub_test_case "reindex" do
    def naptime
      1
    end

    def additional_configurations
      <<-CONFIG
pgroonga_primary_maintainer.naptime = #{naptime}
pgroonga_primary_maintainer.reindex_threshold = 1
      CONFIG
    end

    test "(temporary)" do
      postgresql_log = @postgresql.read_log
      assert_equal(["pgroonga: primary-maintainer: DEBUG pgroonga_primary_maintainer_wal_size_check()"],
                   postgresql_log.scan(/pgroonga: primary-maintainer: DEBUG pgroonga_primary_maintainer_wal_size_check.*$/),
                   postgresql_log)
    end
  end
end
