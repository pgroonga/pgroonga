require_relative "helpers/sandbox"

class ToolsServiceFileGeneratorTestCase < Test::Unit::TestCase
  include Helpers::CommandRunnable

  sub_test_case "generate-pgroonga-primary-maintainer-service" do
    command = "tools/systemd/generate-pgroonga-primary-maintainer-service.sh"

    test "default" do
      expected = <<-EXPECTED
# How to install:
#   tools/systemd/generate-pgroonga-primary-maintainer-service.sh | sudo -H tee /lib/systemd/system/pgroonga-primary-maintainer.service
[Unit]
Description=PGroonga primary maintainer

[Service]
Type=oneshot
User=pgrn
Group=pgrn
Environment=
ExecStart=/tmp/local/bin/pgroonga-primary-maintainer.sh --threshold 1G
[Install]
WantedBy=multi-user.target
      EXPECTED
      assert_equal([expected, ""], run_command(command))
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

      expected = <<-EXPECTED
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
      assert_equal([expected, ""], run_command(*command_line))
    end

    test "generate-pgroonga-primary-maintainer-service.sh is not found" do
      error = assert_raise(Helpers::CommandRunError) do
        run_command({"PATH" => "/usr/bin"}, command)
      end
      assert_equal(["No pgroonga-primary-maintainer.sh command.\n", ""],
                   [error.output, error.error])
    end

    test "help" do
      command_line = [command, "--help"]
      expected = <<-EXPECTED
Options:
--pgroonga-primary-maintainer-command:
  Specify the path to `pgroonga-primary-maintainer.sh`
  (default: /tmp/local/bin/pgroonga-primary-maintainer.sh)
--threshold:
  If the specified value is exceeded, `REINDEX INDEX CONCURRENTLY` is run.
  (default: 1G)
--environment
  Connection information such as `dbname` should be set in environment variables.
  See also: https://www.postgresql.org/docs/current/libpq-envars.html"
  Example: --environment KEY1=VALUE1 --environment KEY2=VALUE2 ...
--psql:
  Specify the path to `psql` command.
--on-failure-service:
  Run SERVICE on failure
--help:
  Display help text and exit.
      EXPECTED
      assert_equal([expected, ""], run_command(*command_line))
    end
  end

  sub_test_case "generate-pgroonga-primary-maintainer-timer" do
    command = "tools/systemd/generate-pgroonga-primary-maintainer-timer.sh"

    test "generate" do
      command_line = [
        command,
        "--time", "1:00",
        "--time", "23:30",
      ]

      expected = <<-EXPECTED
# How to install:
#   tools/systemd/generate-pgroonga-primary-maintainer-timer.sh | sudo -H tee /lib/systemd/system/pgroonga-primary-maintainer.timer
#   sudo -H systemctl daemon-reload
#
# Usage:
#
#   Enable:  sudo -H systemctl enable --now pgroonga-primary-maintainer.timer
#   Disable: sudo -H systemctl disable --now pgroonga-primary-maintainer.timer
[Unit]
Description=PGroonga primary maintainer
[Timer]
OnCalendar=*-*-* 1:00:00
OnCalendar=*-*-* 23:30:00
[Install]
WantedBy=timers.target
      EXPECTED
      assert_equal([expected, ""], run_command(*command_line))
    end

    test "no options" do
      error = assert_raise(Helpers::CommandRunError) do
        run_command(command)
      end
      assert_equal(["Specify run time with `--time`.\n", ""],
                   [error.output, error.error])
    end

    test "help" do
      command_line = [command, "--help"]

      expected = <<-EXPECTED
Options:
--time:
  Specify run time,
  Example: --time 2:00 --time 3:30 ...
--help:
  Display help text and exit.
      EXPECTED
      assert_equal([expected, ""], run_command(*command_line))
    end
  end
end
