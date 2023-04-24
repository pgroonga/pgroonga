require_relative "helpers/sandbox"

class VacuumTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  test "unmap after VACUUM" do
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")
    run_sql("DELETE FROM memos;")
    run_sql("INSERT INTO memos VALUES ('Groonga is good!');")
    thread = Thread.new do
      run_sql("SET pgroonga.log_level = debug; " +
              "SELECT pg_sleep(10); " +
              "SELECT * FROM memos WHERE content &@~ 'groonga';")
    end
    sleep(5)
    run_sql("VACUUM memos;")
    thread.join
    pgroonga_log = @postgresql.read_pgroonga_log
    assert_equal(["pgroonga: unmap DB because VACUUM was executed"],
                 pgroonga_log.scan(/pgroonga: unmap.*$/))
  end
end
