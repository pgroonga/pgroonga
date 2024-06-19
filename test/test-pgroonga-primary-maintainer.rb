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
pgroonga_primary_maintainer.hours = '2,3,4'
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

    test "hours" do
      postgresql_log = @postgresql.read_log
      assert_equal(["pgroonga: primary-maintainer: hours=2,3,4"],
                   postgresql_log.scan(/pgroonga: primary-maintainer: hours=.*$/),
                   postgresql_log)
    end
  end

  sub_test_case "reindex" do
    def naptime
      1
    end

    def additional_configurations
      <<-CONFIG
pgroonga.enable_wal = yes
pgroonga_primary_maintainer.naptime = #{naptime}
pgroonga_primary_maintainer.reindex_threshold = 2
      CONFIG
    end

    setup do
      run_sql("CREATE TABLE notes (content text);")
      run_sql("CREATE INDEX notes_content ON notes USING pgroonga (content);")
      run_sql("INSERT INTO notes VALUES ('PGroonga');")

      run_sql("CREATE TABLE memos (content text);")
      run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
      run_sql("INSERT INTO memos VALUES ('PGroonga');")
    end

    test "nothing" do
      before_log = @postgresql.read_log
      sleep(naptime)
      after_log = @postgresql.read_log
      assert_equal(before_log, after_log)
    end

    test "run" do
      200.times do
        run_sql("INSERT INTO memos VALUES ('PGroonga');")
      end
      sleep(naptime)

      postgresql_log = @postgresql.read_log
      assert_equal(["pgroonga: primary-maintainer: run reindex: memos_content"],
                   postgresql_log.scan(/pgroonga: primary-maintainer: run reindex: memos_content$/).uniq, # uniq is temporary
                   postgresql_log)

      # todo Test for size reduction.
    end

    sub_test_case "within hour" do
      def additional_configurations
        super + <<-CONFIG
pgroonga_primary_maintainer.hours = '#{Time.new.hour},#{(Time.new + 3600).hour}'
        CONFIG
      end

      test "run" do
        200.times do
          run_sql("INSERT INTO memos VALUES ('PGroonga');")
        end
        sleep(naptime)

        postgresql_log = @postgresql.read_log
        assert_equal(["pgroonga: primary-maintainer: run reindex: memos_content"],
                     postgresql_log.scan(/pgroonga: primary-maintainer: run reindex: memos_content$/).uniq, # uniq is temporary
                     postgresql_log)

        # todo Test for size reduction.
      end
    end

    sub_test_case "out of hours" do
      def additional_configurations
        super + <<-CONFIG
pgroonga_primary_maintainer.hours = '#{(Time.new - 3600).hour}'
        CONFIG
      end

      test "nothing" do
        before_log = @postgresql.read_log
        200.times do
          run_sql("INSERT INTO memos VALUES ('PGroonga');")
        end
        sleep(naptime)
        after_log = @postgresql.read_log
        assert_equal(before_log, after_log)
      end
    end
  end
end
