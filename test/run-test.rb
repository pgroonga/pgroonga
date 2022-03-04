#!/usr/bin/env ruby

require "test-unit"

if ENV["NEED_MAKE"] == "yes"
  env = {
    "PGRN_DEBUG" => "1",
    "HAVE_MSGPACK" => "1",
  }
  pg_config = ENV["PG_CONFIG"] || "pg_config"
  n_cpus = Integer(`nproc`.chomp, 10)
  command_line = ["make", "-j#{n_cpus}", "PG_CONFIG=#{pg_config}", "install"]
  command_line.prepend("sudo") if ENV["NEED_SUDO"] == "yes"
  system(env, *command_line, out: IO::NULL) || exit(false)
end

ENV["TEST_UNIT_MAX_DIFF_TARGET_STRING_SIZE"] = "1_000_000"

test_dir = __dir__
exit(Test::Unit::AutoRunner.run(true, test_dir))
