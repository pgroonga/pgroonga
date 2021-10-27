require_relative "helpers/sandbox"

class PGroongaCrashSaferTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  def shared_preload_libraries
    ["pgroonga_crash_safer"]
  end

  def additional_configurations
    "pgroonga.enable_crash_safe = yes"
  end

  test "recover from WAL" do
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    until Dir.glob(File.join(@test_db_dir, "pgrn*.wal")).empty? do
      sleep(0.1)
    end
    Dir.glob(File.join(@test_db_dir, "pgrn*")) do |path|
      FileUtils.cp(path, "#{path}.bak")
    end
    run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")
    Dir.glob(File.join(@test_db_dir, "pgrn*.wal")) do |path|
      FileUtils.cp(path, "#{path}.bak")
    end
    stop_postgres
    Dir.glob(File.join(@test_db_dir, "pgrn*.bak")) do |path|
      FileUtils.cp(path, path.chomp(".bak"))
    end
    start_postgres
    sql = <<-SQL
SET enable_seqscan = no;
SELECT * FROM memos WHERE content &@~ 'PGroonga';
    SQL
    assert_equal([<<-OUTPUT, ""], run_sql(sql))
#{sql}
      content      
-------------------
 PGroonga is good!
(1 row)

    OUTPUT
  end

  test "recover by REINDEX" do
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")
    stop_postgres
    File.open(File.join(@test_db_dir, "pgrn"), "w") do |pgrn|
      pgrn.puts("Broken")
    end
    start_postgres
    sql = <<-SQL
SET enable_seqscan = no;
SELECT * FROM memos WHERE content &@~ 'PGroonga';
    SQL
    assert_equal([<<-OUTPUT, ""], run_sql(sql))
#{sql}
      content      
-------------------
 PGroonga is good!
(1 row)

    OUTPUT
  end
end
