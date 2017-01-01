require_relative "helpers/sandbox"

class PGroongaCheckTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  sub_test_case "broken" do
    sub_test_case "DB" do
      test "can't open" do
        run_sql("CREATE TABLE memos (content text);")
        run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
        run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")
        stop_postgres
        File.open(File.join(@test_db_dir, "pgrn"), "w") do |pgrn|
          pgrn.puts("Broken")
        end
        start_postgres
        output = <<-OUTPUT
SET enable_seqscan = no;
SELECT * FROM memos WHERE content %% 'PGroonga';
      content      
-------------------
 PGroonga is good!
(1 row)

        OUTPUT
        assert_equal([output, ""],
                     run_sql("SET enable_seqscan = no;\n" +
                             "SELECT * FROM memos WHERE content %% 'PGroonga';"))
      end
    end
  end
end
