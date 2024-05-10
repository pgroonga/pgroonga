require_relative "helpers/sandbox"

class PGroongaWALResourceManagerTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  def additional_configurations
    <<-CONFIG
pgroonga.enable_wal_resource_manager = yes
    CONFIG
  end

  def shared_preload_libraries
    [
      # We may use this later.
      # "pgroonga_wal_resource_manager",
    ]
  end

  def shared_preload_libraries_standby
    ["pgroonga_wal_resource_manager"]
  end

  setup :setup_standby_db
  teardown :teardown_standby_db

  test "create table" do
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")

    select = <<-SELECT
SELECT pgroonga_command('object_exist',
                        ARRAY[
                          'name', 'Building' || pgroonga_table_name('memos_content')
                        ])::jsonb->1;
SELECT
    output = <<-OUTPUT
#{select};
 ?column? 
----------
 true
(1 row)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{select};"))
  end
end
