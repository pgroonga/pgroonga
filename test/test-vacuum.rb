require_relative "helpers/sandbox"

class VacuumTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  def groonga_table_names
    table_list = run_sql("SELECT pgroonga_command('table_list');")
    JSON.parse(table_list[0][/(\[.+\])/, 1])[1][1..-1].collect do |table|
      table[1]
    end
  end

  test "unmap after VACUUM" do
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")
    run_sql("DELETE FROM memos;")
    run_sql("INSERT INTO memos VALUES ('Groonga is good!');")
    run_sql do |input, output, error|
      input.puts("SET pgroonga.log_level = debug;")
      input.puts("SELECT pgroonga_command('status');")
      output.each_line do |line|
        break if line.strip.empty?
      end
      run_sql("SET pgroonga.log_level = debug;",
              "SELECT pgroonga_command('log_put debug \"before VACUUM\"');",
              "VACUUM memos;",
              "SELECT pgroonga_command('log_put debug \"after VACUUM\"');")
      input.puts("SELECT pgroonga_command('log_put debug \"before SELECT\"');")
      input.puts("SELECT * FROM memos WHERE content &@~ 'groonga';")
      input.puts("SELECT pgroonga_command('log_put debug \"after SELECT\"');")
      input.close
    end
    pgroonga_log = @postgresql.read_pgroonga_log
    assert_equal(["pgroonga: unmap DB because VACUUM was executed"],
                 pgroonga_log.scan(/pgroonga: unmap.*$/),
                 pgroonga_log)
  end

  test "broken object" do
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")
    run_sql("INSERT INTO memos VALUES ('PGroonga is good!');")
    table_list = run_sql("SELECT pgroonga_command('table_list');")
    table_list = JSON.parse(table_list[0][/^ *(\[.*)$/, 1])[1][1..-1]
    table_name = run_sql("SELECT pgroonga_table_name('memos_content');")
    table_name = table_name[0][/^ *(Sources.*)$/, 1]
    table_path = table_list.find {|_, name,| name == table_name}[2]
    table_path = File.join(@postgresql.dir, table_path)
    run_sql("REINDEX INDEX memos_content;")
    run_sql("SELECT pgroonga_command('database_unmap');")
    FileUtils.rm(table_path)
    run_sql("VACUUM memos;")
  end

  test "keep objects of CREATE INDEX in progress" do
    run_sql("CREATE TABLE copy_memos (content text);")
    run_sql("CREATE INDEX copy_content ON copy_memos USING pgroonga (content);")
    run_sql("CREATE TABLE memos (content text);")

    sources_table_name = nil
    run_sql do |input, output, error|
      input.puts("BEGIN;")
      input.puts("CREATE INDEX memos_content ON memos USING pgroonga (content);")
      input.puts("SELECT pgroonga_table_name('memos_content');")
      output.each_line do |line|
        sources_table_name = $1 if /^ *(Sources\d+)$/ =~ line
        break if line.strip.empty?
      end

      # Test whether `sources_table_name` remains by running VACUUM during a commit.
      # Related issue:
      # https://github.com/pgroonga/pgroonga/issues/1013
      run_sql("VACUUM copy_memos;")

      assert_include(groonga_table_names, sources_table_name)

      input.puts("COMMIT;")
      input.close
    end
  end

  test "remove objects of rollbacked CREATE INDEX" do
    run_sql("CREATE TABLE copy_memos (content text);")
    run_sql("CREATE INDEX copy_content ON copy_memos USING pgroonga (content);")
    run_sql("CREATE TABLE memos (content text);")

    sources_table_name = nil
    run_sql do |input, output, error|
      input.puts("BEGIN;")
      input.puts("CREATE INDEX memos_content ON memos USING pgroonga (content);")
      input.puts("SELECT pgroonga_table_name('memos_content');")
      output.each_line do |line|
        sources_table_name = $1 if /^ *(Sources\d+)$/ =~ line
        break if line.strip.empty?
      end

      input.puts("ROLLBACK;")
      input.close
    end

    # The objects of rollbacked CREATE INDEX remain until VACUUM removes them.
    run_sql("VACUUM copy_memos;")
    assert_not_include(groonga_table_names, sources_table_name)
  end

  test "sequential search + NormalizerTable" do
    run_sql("CREATE TABLE normalizations (target text, normalized text);")
    run_sql(<<-SQL)
CREATE INDEX pgroonga_normalizations_index ON normalizations
  USING pgroonga (target pgroonga_text_term_search_ops_v2,
                  normalized);
    SQL
    run_sql("INSERT INTO normalizations VALUES ('o', '0');")
    run_sql("INSERT INTO normalizations VALUES ('ss', '55');")

    run_sql("CREATE TABLE memos (id integer, content text);")
    run_sql(<<-SQL)
CREATE INDEX pgroonga_memos_content ON memos
 USING pgroonga (content)
  WITH (normalizers='
          NormalizerNFKC150,
          NormalizerTable(
            "normalized", "${table:public.pgroonga_normalizations_index}.normalized",
            "target", "target"
          )
        ');
    SQL
    run_sql("INSERT INTO memos VALUES (1, '0123455');")
    run_sql("INSERT INTO memos VALUES (2, '01234ss');")

    select = <<-SELECT.chomp
SELECT * FROM memos
 WHERE content &@~ (
       'o123455',
       NULL,
       'pgroonga_memos_content'
     )::pgroonga_full_text_search_condition;
    SELECT
    result = run_sql do |input, output, error|
      input.puts("SET enable_indexscan = no;")
      input.puts("SET enable_bitmapscan = no;")
      input.puts(select)
      output.each_line do |line|
        break if line.strip.empty?
      end
      run_sql("VACUUM memos;")
      input.puts(select)
      input.close
    end
    assert_equal([<<-OUTPUT, ""], result)
#{select}
 id | content 
----+---------
  1 | 0123455
  2 | 01234ss
(2 rows)

    OUTPUT
  end

  test "highlight_html + NormalizerTable" do
    run_sql("CREATE TABLE normalizations (target text, normalized text);")
    run_sql(<<-SQL)
CREATE INDEX pgroonga_normalizations_index ON normalizations
  USING pgroonga (target pgroonga_text_term_search_ops_v2,
                  normalized);
    SQL
    run_sql("INSERT INTO normalizations VALUES ('o', '0');")
    run_sql("INSERT INTO normalizations VALUES ('ss', '55');")

    run_sql("CREATE TABLE memos (id integer, content text);")
    run_sql(<<-SQL)
CREATE INDEX pgroonga_memos_content ON memos
 USING pgroonga (content)
  WITH (normalizers='
          NormalizerNFKC150,
          NormalizerTable(
            "normalized", "${table:public.pgroonga_normalizations_index}.normalized",
            "target", "target"
          )
        ');
    SQL
    run_sql("INSERT INTO memos VALUES (1, '0123455');")
    run_sql("INSERT INTO memos VALUES (2, '01234ss');")

    highlight = <<-HIGHLIGHT.chomp
SELECT pgroonga_highlight_html(
         content,
         ARRAY['o12', '455'],
         'pgroonga_memos_content'
       )
  FROM memos;
    HIGHLIGHT
    result = run_sql do |input, output, error|
      input.puts(highlight)
      output.each_line do |line|
        break if line.strip.empty?
      end
      run_sql("VACUUM memos;")
      input.puts(<<-SELECT_TO_INVOKE_UNMAP)
SET enable_indexscan = no;
SET enable_bitmapscan = no;
SELECT * FROM memos
 WHERE content &@~ (
       'o123455',
       NULL,
       'pgroonga_memos_content'
     )::pgroonga_full_text_search_condition;
      SELECT_TO_INVOKE_UNMAP
      output.each_line do |line|
        break if line.strip.empty?
      end
      input.puts(highlight)
      input.close
    end
    assert_equal([<<-OUTPUT, ""], result)
#{highlight}
 pgroonga_highlight_html 
-------------------------
 0123455
 01234ss
(2 rows)

    OUTPUT
  end
end
