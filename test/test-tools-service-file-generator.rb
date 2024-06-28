require_relative "helpers/sandbox"

class ToolsServiceFileGeneratorTestCase < Test::Unit::TestCase
  include Helpers::CommandRunnable

  PRIMARY_MAINTAINER_COMMAND = "pgroonga-primary-maintainer.sh"

  setup do
    omit("Support for Linux only.") unless RUBY_PLATFORM.include?("linux")
  end

  sub_test_case "pgroonga-generate-primary-maintainer-service" do
    command = "pgroonga-generate-primary-maintainer-service.sh"

    test "default" do
      expected = <<-EXPECTED
# How to install:
#   #{find_command(command)} | sudo -H tee /lib/systemd/system/pgroonga-primary-maintainer.service
[Unit]
Description=PGroonga primary maintainer

[Service]
Type=oneshot
User=#{Etc.getpwuid(Process.uid).name}
Group=#{Etc.getgrgid(Process.gid).name}
Environment=
ExecStart=#{find_command(PRIMARY_MAINTAINER_COMMAND)} --threshold 1G
[Install]
WantedBy=multi-user.target
      EXPECTED
      assert_equal([expected, ""], run_command(command))
    end

    test "full options" do
      pgroonga_primary_maintainer_command = "tools/pgroonga-primary-maintainer.sh"
      command_line = [
        command,
        "--pgroonga-primary-maintainer-command", pgroonga_primary_maintainer_command,
        "--threshold", "5G",
        "--environment", "PGHOST=localhost",
        "--environment", "PGDATABASE=test_db",
        "--psql", "psql-path",
        "--on-failure-service", "on-failure"
      ]

      expected = <<-EXPECTED
# How to install:
#   #{find_command(command)} | sudo -H tee /lib/systemd/system/pgroonga-primary-maintainer.service
[Unit]
Description=PGroonga primary maintainer
OnFailure=on-failure
[Service]
Type=oneshot
User=#{Etc.getpwuid(Process.uid).name}
Group=#{Etc.getgrgid(Process.gid).name}
Environment=PGHOST=localhost PGDATABASE=test_db
ExecStart=#{File.expand_path(pgroonga_primary_maintainer_command)} --threshold 5G --psql psql-path
[Install]
WantedBy=multi-user.target
      EXPECTED
      assert_equal([expected, ""], run_command(*command_line))
    end

    test "pgroonga-primary-maintainer.sh is not found" do
      error = assert_raise(Helpers::CommandRunError) do
        run_command({"PATH" => "/usr/bin"}, find_command(command))
      end
      assert_equal(["Specify the path of pgroonga-primary-maintainer.sh with '--pgroonga-primary-maintainer-command'\n", ""],
                   [error.output, error.error])
    end

    test "help" do
      command_line = [command, "--help"]
      expected = <<-EXPECTED
Options:
--pgroonga-primary-maintainer-command:
  Specify the path to `pgroonga-primary-maintainer.sh`
  (default: #{find_command(PRIMARY_MAINTAINER_COMMAND)})
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

  sub_test_case "pgroonga-generate-primary-maintainer-timer" do
    command = "pgroonga-generate-primary-maintainer-timer.sh"

    test "generate" do
      command_line = [
        command,
        "--time", "1:00",
        "--time", "23:30",
      ]

      expected = <<-EXPECTED
# How to install:
#   #{find_command(command)} | sudo -H tee /lib/systemd/system/pgroonga-primary-maintainer.timer
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
