require_relative "helpers/sandbox"

class ReindexTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  test "REINDEX CONCURRENTLY with UPDATE" do
    unless @postgresql.version < 12
      omit("REINDEX CONCURRENTLY is available since PostgreSQL 12")
    end
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    run_sql("INSERT INTO memos " +
            "SELECT i::text FROM generate_series(1, 1000) AS i;")
    3.times do |i|
      reindex_finished = false
      reindex_thread = Thread.new do
        begin
          run_sql("REINDEX INDEX CONCURRENTLY memos_content;")
        ensure
          reindex_finished = true
        end
      end
      until reindex_finished
        run_sql("UPDATE memos SET content = content || content;")
      end
      reindex_thread.join
    end
  end
end
