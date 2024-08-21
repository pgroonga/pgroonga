require_relative "helpers/sandbox"

class QueryExpandTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  def groonga_synonym_generate(*args)
    begin
      output = Tempfile.new(["groonga-synonym-generate", ".sql"])
      run_command("groonga-synonym-generate",
                  "--output", output.path,
                  *args)
      output
    rescue
      omit("groonga-synonym-generate isn't available")
    end
  end

  sub_test_case("Sudachi") do
    setup do
      if windows?
        omit("Can't use multibyte characters on Windows")
      end
      run_sql(<<-SQL)
CREATE TABLE thesaurus (
  term text PRIMARY KEY,
  synonyms text[]
);
             SQL
      run_sql(<<-SQL)
CREATE INDEX thesaurus_term_index
  ON thesaurus
  USING pgroonga (term pgroonga_text_term_search_ops_v2);
              SQL
      output = groonga_synonym_generate("--format", "pgroonga")
      assert_equal("", run_sql(<<-SQL)[1])
\\i #{output.path}
              SQL
      run_sql(<<-SQL)
CREATE TABLE memos (
  content text
);
              SQL
      run_sql(<<-SQL)
CREATE INDEX memos_content_index
  ON memos
  USING pgroonga (content);
              SQL
    end

    def test_included_word
      run_sql(<<-SQL)
INSERT INTO memos VALUES
  ('キャパシティー'),
  ('キャパ'),
  ('容量'),
  ('unrelated');
              SQL
    assert_equal([<<-EXPECTED, ""],
SELECT content, round(pgroonga_score(tableoid, ctid)::numeric, 2) AS score
  FROM memos
 WHERE content &@~ pgroonga_query_expand('thesaurus',
                                         'term',
                                         'synonyms',
                                         'キャパシティー')
ORDER BY score DESC,
         content;

    content     | score 
----------------+-------
 キャパシティー |  1.00
 キャパ         |  0.80
 容量           |  0.60
(3 rows)

                 EXPECTED
                 run_sql(<<-SQL))
SELECT content, round(pgroonga_score(tableoid, ctid)::numeric, 2) AS score
  FROM memos
 WHERE content &@~ pgroonga_query_expand('thesaurus',
                                         'term',
                                         'synonyms',
                                         'キャパシティー')
ORDER BY score DESC,
         content;
                         SQL
    end

    def test_with_user_thesaurus
      run_sql(<<-SQL)
CREATE TABLE user_thesaurus (
  term text PRIMARY KEY,
  synonyms text[]
);
              SQL
      run_sql(<<-SQL)
CREATE INDEX user_thesaurus_term_index
  ON user_thesaurus
  USING pgroonga (term pgroonga_text_term_search_ops_v2);
              SQL
      run_sql(<<-SQL)
INSERT INTO user_thesaurus VALUES
  ('きゃぱ', ARRAY['きゃぱ', '>-0.2キャパ']);
              SQL
      run_sql(<<-SQL)
INSERT INTO memos VALUES
  ('キャパシティー'),
  ('キャパ'),
  ('きゃぱ'),
  ('容量'),
  ('unrelated');
              SQL
    assert_equal([<<-EXPECTED, ""],
SELECT content, round(pgroonga_score(tableoid, ctid)::numeric, 2) AS score
  FROM memos
 WHERE content &@~
         pgroonga_query_expand(
           'thesaurus',
           'term',
           'synonyms',
           pgroonga_query_expand(
             'user_thesaurus',
             'term',
             'synonyms',
             'きゃぱ'))
ORDER BY score DESC,
         content;

    content     | score 
----------------+-------
 きゃぱ         |  1.00
 キャパ         |  0.80
 キャパシティー |  0.80
 容量           |  0.40
(4 rows)

                 EXPECTED
                 run_sql(<<-SQL))
SELECT content, round(pgroonga_score(tableoid, ctid)::numeric, 2) AS score
  FROM memos
 WHERE content &@~
         pgroonga_query_expand(
           'thesaurus',
           'term',
           'synonyms',
           pgroonga_query_expand(
             'user_thesaurus',
             'term',
             'synonyms',
             'きゃぱ'))
ORDER BY score DESC,
         content;
                         SQL
    end
  end
end
