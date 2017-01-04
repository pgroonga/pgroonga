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

    sub_test_case "index" do
      test "locked" do
        run_sql("CREATE TABLE memos (content text);")
        run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
        run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")
        stop_postgres
        lexicon = nil
        groonga("table_list")[1][2..-1].each do |_, name,|
          if name.start_with?("Lexicon")
            lexicon = name
            break
          end
        end
        groonga("lock_acquire", "#{lexicon}.index")
        start_postgres
        run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")
        output = <<-OUTPUT
SET enable_seqscan = no;
SELECT * FROM memos WHERE content %% 'PGroonga';
      content      
-------------------
 PGroonga is good!
 PGroonga is good!
(2 rows)

        OUTPUT
        assert_equal([output, ""],
                     run_sql("SET enable_seqscan = no;\n" +
                             "SELECT * FROM memos WHERE content %% 'PGroonga';"))
      end
    end
  end
end
