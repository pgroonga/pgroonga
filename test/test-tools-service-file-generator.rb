require_relative "helpers/sandbox"

class ToolsServiceFileGeneratorTestCase < Test::Unit::TestCase
  include Helpers::CommandRunnable

  sub_test_case "generate-pgroonga-primary-maintainer-service" do
    command = "tools/systemd/generate-pgroonga-primary-maintainer-service.sh"

    test "default" do
      output, _ =  run_command(command)
      assert_equal(<<-EXPECTED, output)
# How to install:
#   tools/systemd/generate-pgroonga-primary-maintainer-service.sh | sudo -H tee /lib/systemd/system/pgroonga-primary-maintainer.service
[Unit]
Description=PGroonga primary maintainer

[Service]
Type=oneshot
User=pgrn
Group=pgrn
Environment=
ExecStart=/tmp/local/bin//pgroonga-primary-maintainer.sh --threshold 1G
[Install]
WantedBy=multi-user.target
                   EXPECTED
    end

    test "full options" do
      command_line = [
        command,
        "--pgroonga-primary-maintainer-command", "tools/pgroonga-primary-maintainer.sh",
        "--threshold", "5G",
        "--environment", "PGHOST=localhost",
        "--environment", "PGDATABASE=test_db",
        "--psql", "psql-path",
        "--on-failure-service", "on-failure"
      ]

      output, _ =  run_command(*command_line)
      assert_equal(<<-EXPECTED, output)
# How to install:
#   tools/systemd/generate-pgroonga-primary-maintainer-service.sh | sudo -H tee /lib/systemd/system/pgroonga-primary-maintainer.service
[Unit]
Description=PGroonga primary maintainer
OnFailure=on-failure
[Service]
Type=oneshot
User=pgrn
Group=pgrn
Environment=PGHOST=localhost PGDATABASE=test_db
ExecStart=tools/pgroonga-primary-maintainer.sh --threshold 5G --psql psql-path
[Install]
WantedBy=multi-user.target
                   EXPECTED
    end

    test "generate-pgroonga-primary-maintainer-service.sh is not found" do
      output = nil
      begin
        run_command({"PATH" => "/usr/bin"}, command)
      rescue Helpers::CommandRunError => err
        output = err.output
      end

      assert_equal(<<-EXPECTED, output)
No pgroonga-primary-maintainer.sh command.
                   EXPECTED
    end

    test "help" do
      command_line = [command, "--help"]
      output, _ = run_command(*command_line)

      assert_equal(<<-EXPECTED, output)
Options:
-s, --pgroonga-primary-maintainer-command:
  Specify the path to `pgroonga-primary-maintainer.sh`
  (default: /tmp/local/bin//pgroonga-primary-maintainer.sh)
-t, --threshold:
  If the specified value is exceeded, `REINDEX INDEX CONCURRENTLY` is run.
  (default: 1G)
-e, --environment
  Connection information such as `dbname` should be set in environment variables.
  See also: https://www.postgresql.org/docs/current/libpq-envars.html"
  Example: --environment KEY1=VALUE1 --environment KEY2=VALUE2 ...
-c, --psql:
  Specify the path to `psql` command.
-f, --on-failure-service:
  Run SERVICE on failure
-h, --help:
  Display help text and exit.
                   EXPECTED
    end
  end
end
