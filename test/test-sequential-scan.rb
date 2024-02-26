require_relative "helpers/sandbox"

class SequentialScanTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  def execute(input, output, sql)
    input.puts(sql)
    input.flush
    read_command_output_all(output)
  end

  # This "vacuum" is not related to PostgreSQL's VACUUM command. We
  # use "vacuum" here to remove unused data for sequential scan
  # periodically.
  test "vacuum" do
    run_sql do |input, output, error|
      execute(input, output, "\\unset ECHO")
      execute(input, output, "SET pgroonga.log_level = debug;")
      execute(input, output, "SET enable_bitmapscan = off;")
      execute(input, output, "SET enable_indexscan = off;")
      execute(input, output, "CREATE TABLE memos (title text, content text);")
      execute(input, output,
              "CREATE INDEX memos_title ON memos " +
              "USING pgroonga (title);")
      execute(input, output,
              "CREATE INDEX memos_index ON memos " +
              "USING pgroonga (title, content);")
      execute(input, output, "INSERT INTO memos VALUES ('hello', 'world');")
      # New cache (#1) is created.
      execute(input, output, <<-SELECT)
SELECT * FROM memos
 WHERE content &@ ('heLl', null, 'memos_title')::pgroonga_full_text_search_condition;
      SELECT
      # The same cache (#1) is used with the same index. Query isn't cared.
      execute(input, output, <<-SELECT)
SELECT * FROM memos
 WHERE content &@ ('world', null, 'memos_title')::pgroonga_full_text_search_condition;
      SELECT
      # New cache (#2) is created for different index.
      execute(input, output, <<-SELECT)
SELECT * FROM memos
 WHERE content &@ ('world', null, 'memos_index')::pgroonga_full_text_search_condition;
      SELECT
      # New cache (#3) is created for different attribute name.
      execute(input, output, <<-SELECT)
SELECT * FROM memos
 WHERE content &@ ('world', null, 'memos_index.title')::pgroonga_full_text_search_condition;
      SELECT
      # The same cache (#3) is used with the same index and attribute.
      execute(input, output, <<-SELECT)
SELECT * FROM memos
 WHERE content &@ ('pen', null, 'memos_index.title')::pgroonga_full_text_search_condition;
      SELECT
      # 100 queries are needed to trigger a "vacuum".
      100.times do
        execute(input, output, "SELECT 1;")
      end
      pattern = /\[release\]\[sequential-search\]\[(?:start|end)\] \d+/
      log = @postgresql.read_pgroonga_log
      assert_equal([
                     # All caches are remained.
                     "[release][sequential-search][start] 3",
                     "[release][sequential-search][end] 3",
                   ],
                   log.scan(pattern))
      # cache #1 is used again.
      execute(input, output, <<-SELECT)
SELECT * FROM memos
 WHERE content &@ ('world', null, 'memos_title')::pgroonga_full_text_search_condition;
      SELECT
      # cache #3 is used again.
      execute(input, output, <<-SELECT)
SELECT * FROM memos
 WHERE content &@ ('world', null, 'memos_index.title')::pgroonga_full_text_search_condition;
      SELECT
      100.times do
        execute(input, output, "SELECT 1;")
      end
      log = @postgresql.read_pgroonga_log
      assert_equal([
                     # The 1st log.
                     "[release][sequential-search][start] 3",
                     "[release][sequential-search][end] 3",
                     # Used caches (#1 and #3) are remained.
                     "[release][sequential-search][start] 3",
                     "[release][sequential-search][end] 2",
                   ],
                   log.scan(pattern))
      100.times do
        execute(input, output, "SELECT 1;")
      end
      log = @postgresql.read_pgroonga_log
      assert_equal([
                     # The 1st log.
                     "[release][sequential-search][start] 3",
                     "[release][sequential-search][end] 3",
                     # The 2nd log.
                     "[release][sequential-search][start] 3",
                     "[release][sequential-search][end] 2",
                     # All caches are vacuumed.
                     "[release][sequential-search][start] 2",
                     "[release][sequential-search][end] 0",
                   ],
                   log.scan(pattern))
      # cache #3 is vacuumed.
      select = <<-SELECT
SELECT * FROM memos
 WHERE content &@ ('world', null, 'memos_index.title')::pgroonga_full_text_search_condition;
      SELECT
      assert_equal(<<-OUTPUT, execute(input, output, select))
 title | content 
-------+---------
 hello | world
(1 row)

      OUTPUT
      input.close
    end
  end
end
