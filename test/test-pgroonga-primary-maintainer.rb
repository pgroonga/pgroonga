require_relative "helpers/sandbox"

class PGroongaPrimaryMaintainerTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  PRIMARY_MAINTAINER_COMMAND = "pgroonga-primary-maintainer.sh"

  def run_primary_maintainer_command(*options)
    env = {
      "PGHOST" => @postgresql.host,
      "PGPORT" => @postgresql.port.to_s,
      "PGDATABASE" => @test_db_name,
      "PGUSER" => @postgresql.user
    }
    commane_line = [PRIMARY_MAINTAINER_COMMAND] + options
    run_command(env, *commane_line)
  end

  def additional_configurations
    "pgroonga.enable_wal = yes"
  end

  setup do
    omit("Omit on Windows: Bash scripts cannot be run.") if windows?

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
    options = ["-t", "1048576"]
    run_primary_maintainer_command(*options)
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
    options = ["-t", "8192"]
    run_primary_maintainer_command(*options)
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

  test "reindex (numfmt)" do
    omit("Require numfmt.") unless RUBY_PLATFORM.include?("linux")

    options = ["--threshold", "16K"]
    run_primary_maintainer_command(*options)
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

  test "help" do
    command_line = [PRIMARY_MAINTAINER_COMMAND, "-h"]

    if RUBY_PLATFORM.include?("linux")
      threshold_example = "--threshold 10M, -t 1G"
    else
      threshold_example = "-t 10485760"
    end

    expected = <<-EXPECTED
#{find_command(PRIMARY_MAINTAINER_COMMAND)} --threshold REINDEX_THRESHOLD_SIZE [--psql PSQL_COMMAND_PATH]

Options:
-t, --threshold:
  If the specified value is exceeded, `REINDEX INDEX CONCURRENTLY` is run.
  Specify by size.
  Example: #{threshold_example}
-c, --psql:
  Specify the path to `psql` command.
-h, --help:
  Display help text and exit.

Connection information such as `dbname` should be set in environment variables.
See also: https://www.postgresql.org/docs/current/libpq-envars.html
    EXPECTED
    assert_equal([expected, ""], run_command(*command_line))
  end
end
