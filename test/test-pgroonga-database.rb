require_relative "helpers/sandbox"

class PGroongaDatabaseTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  sub_test_case "pgroonga_database_remove" do
    test "tablespace" do
      tablespace_location = File.join(@tmp_dir, "tablespace")
      FileUtils.mkdir_p(tablespace_location)
      system("dir #{tablespace_location}")
      pgrn_pattern = File.join(@test_db_dir, "pgrn.*")
      tablespace_pgrn_pattern =
        File.join(tablespace_location, "PG_*", "*", "pgrn.*")
      run_sql("CREATE TABLESPACE space LOCATION '#{tablespace_location}'");
      run_sql("CREATE TABLE memos (content text);")
      run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content) " +
              "TABLESPACE space;")
      run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")
      assert do
        not Dir.glob(pgrn_pattern).empty?
      end
      assert do
        not Dir.glob(tablespace_pgrn_pattern).empty?
      end
      stop_postgres
      File.open(File.join(@test_db_dir, "pgrn"), "w") do |pgrn|
        pgrn.puts("Broken")
      end
      start_postgres
      output = <<-OUTPUT
CREATE EXTENSION pgroonga_database;
SELECT pgroonga_database_remove();
 pgroonga_database_remove 
--------------------------
 t
(1 row)

      OUTPUT
      assert_equal([output, ""],
                   run_sql("CREATE EXTENSION pgroonga_database;\n" +
                           "SELECT pgroonga_database_remove();"))
      assert do
        Dir.glob(pgrn_pattern).empty?
      end
      assert do
        Dir.glob(tablespace_pgrn_pattern).empty?
      end
    end
  end
end
