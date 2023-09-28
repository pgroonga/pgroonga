require_relative "helpers/sandbox"

class SequentialScanTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  # This "vacuum" is not related to PostgreSQL's VACUUM command. We
  # use "vacuum" here to remove unused data for sequential scan
  # periodically.
  test "vacuum" do
    run_sql do |input, output, error|
      input.puts("SET pgroonga.log_level = debug;")
      read_command_output_all(output)
      input.puts("SET enable_bitmapscan = off;")
      read_command_output_all(output)
      input.puts("SET enable_indexscan = off;")
      read_command_output_all(output)
      input.puts("CREATE TABLE memos (title text, content text);")
      read_command_output_all(output)
      input.puts("CREATE INDEX memos_title ON memos " +
                 "USING pgroonga (title);")
      read_command_output_all(output)
      input.puts("CREATE INDEX memos_index ON memos " +
                 "USING pgroonga (title, content);")
      read_command_output_all(output)
      input.puts("INSERT INTO memos VALUES ('hello', 'world');")
      read_command_output_all(output)
      # New cache is created.
      input.puts(<<-SELECT)
SELECT * FROM memos
 WHERE content &@ ('heLl', null, 'memos_title')::pgroonga_full_text_search_condition;
      SELECT
      read_command_output_all(output)
      # The same cache is used with the same index. Query isn't cared.
      input.puts(<<-SELECT)
SELECT * FROM memos
 WHERE content &@ ('world', null, 'memos_title')::pgroonga_full_text_search_condition;
      SELECT
      read_command_output_all(output)
      # New cache is created for different index.
      input.puts(<<-SELECT)
SELECT * FROM memos
 WHERE content &@ ('world', null, 'memos_index')::pgroonga_full_text_search_condition;
      SELECT
      read_command_output_all(output)
      # New cache is created for different attribute name.
      input.puts(<<-SELECT)
SELECT * FROM memos
 WHERE content &@ ('world', null, 'memos_index.title')::pgroonga_full_text_search_condition;
      SELECT
      read_command_output_all(output)
      # The same cache is used with the same index and attribute.
      input.puts(<<-SELECT)
SELECT * FROM memos
 WHERE content &@ ('pen', null, 'memos_index.title')::pgroonga_full_text_search_condition;
      SELECT
      read_command_output_all(output)
      # 100 queries are needed to trigger a "vacuum".
      100.times do
        input.puts("SELECT 1;")
        read_command_output_all(output)
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
      input.puts(<<-SELECT)
SELECT * FROM memos
 WHERE content &@ ('world', null, 'memos_content')::pgroonga_full_text_search_condition;
      SELECT
      # cache #3 is used again.
      input.puts(<<-SELECT)
SELECT * FROM memos
 WHERE content &@ ('world', null, 'memos_index.title')::pgroonga_full_text_search_condition;
      SELECT
      read_command_output_all(output)
      100.times do
        input.puts("SELECT 1;")
        read_command_output_all(output)
      end
      log = @postgresql.read_pgroonga_log
      assert_equal([
                     # The previous log.
                     "[release][sequential-search][start] 3",
                     "[release][sequential-search][end] 3",
                     # Used cache is remained.
                     "[release][sequential-search][start] 3",
                     "[release][sequential-search][end] 1",
                   ],
                   log.scan(pattern))
      input.close
    end
  end
end
