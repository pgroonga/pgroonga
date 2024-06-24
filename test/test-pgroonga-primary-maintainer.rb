require_relative "helpers/sandbox"

class PGroongaPrimaryMaintainerTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  PRIMARY_MAINTAINER_COMMAND = "pgroonga-primary-maintainer.sh"

  def run_primary_maintainer_command(*options)
    psql_options = [
      "--host", @postgresql.host,
      "--port", @postgresql.port.to_s,
      "--username", @postgresql.user,
    ]
    commane_line = [
      PRIMARY_MAINTAINER_COMMAND,
      "--dbname", @test_db_name,
      "--psql_options", psql_options.join(" ")
    ] + options
    run_command(*commane_line)
  end

  def additional_configurations
    "pgroonga.enable_wal = yes"
  end

  setup do
    run_sql("CREATE TABLE notes (content text);")
    run_sql("CREATE INDEX notes_content ON notes USING pgroonga (content);")
    run_sql("INSERT INTO notes VALUES ('PGroonga');")

    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    run_sql("INSERT INTO memos VALUES ('PGroonga');")
    200.times do
      run_sql("INSERT INTO memos VALUES ('PGroonga');")
    end
    run_sql("DELETE FROM memos;")
  end

  test "nothing" do
    run_primary_maintainer_command('--thresholds', '1M')
    assert_equal([<<-EXPECTED, ""],
SELECT name, last_block FROM pgroonga_wal_status()
     name      | last_block 
---------------+------------
 notes_content |          1
 memos_content |          2
(2 rows)

                 EXPECTED
                 run_sql("SELECT name, last_block FROM pgroonga_wal_status()"))
  end

  test "reindex" do
    run_primary_maintainer_command('--thresholds', '8192')
    assert_equal([<<-EXPECTED, ""],
SELECT name, last_block FROM pgroonga_wal_status()
     name      | last_block 
---------------+------------
 notes_content |          1
 memos_content |          1
(2 rows)

                 EXPECTED
                 run_sql("SELECT name, last_block FROM pgroonga_wal_status()"))
  end
end
