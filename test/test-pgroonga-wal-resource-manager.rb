require_relative "helpers/sandbox"

class PGroongaWALResourceManagerTestCase < Test::Unit::TestCase
  include Helpers::Sandbox

  setup def check_postgresql_version
    if @postgresql.version < 15
      omit("custom WAL resource manager is available since PostgreSQL 15")
    end
  end

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

  test "create column" do
    run_sql("CREATE TABLE memos (content text);")
    run_sql("CREATE INDEX memos_content ON memos USING pgroonga (content);")

    # TODO
    sleep(0.1)
    select = <<-SELECT
SELECT pgroonga_command('object_inspect',
                        ARRAY[
                          'name', pgroonga_index_column_name('memos_content', 'content')
                        ])::jsonb->1->'sources'->0->'name';
SELECT
    output = <<-OUTPUT
#{select};
 ?column?  
-----------
 "content"
(1 row)

    OUTPUT
    assert_equal([output, ""],
                 run_sql_standby("#{select};"))
  end
end
