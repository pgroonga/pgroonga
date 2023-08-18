require_relative "helpers/sandbox"

class ReindexTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  test "REINDEX CONCURRENTLY with UPDATE" do
    if @postgresql.version < 12
      omit("REINDEX CONCURRENTLY is available since PostgreSQL 12")
    end
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    run_sql("INSERT INTO memos " +
            "SELECT i::text FROM generate_series(1, 1000) AS i;")
    3.times do
      result = run_sql do |input, output, error|
        input.puts("REINDEX INDEX CONCURRENTLY memos_content;")
        output.gets
        loop do
          readables, = IO.select([output, error], nil, nil, 0)
          break if readables
          run_sql("UPDATE memos SET content = content || content;")
        end
        input.close
      end
      assert_equal([<<-OUTPUT, ""], result)
REINDEX
      OUTPUT
    end
  end
end
