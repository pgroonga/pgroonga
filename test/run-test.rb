#!/usr/bin/env ruby

require "test-unit"

if ENV["NEED_BUILD"] == "yes"
  build_dir = ENV["BUILD_DIR"] || Dir.pwd
  system("meson", "compile", "-C", build_dir) || exit(false)
  install_command = ["meson", "install", "-C", build_dir, "--no-rebuild"]
  install_command.prepend("sudo", "-H") if ENV["NEED_SUDO"] == "yes"
  system(*install_command, out: IO::NULL) || exit(false)
end

ENV["TEST_UNIT_MAX_DIFF_TARGET_STRING_SIZE"] = "1_000_000"

test_dir = __dir__
exit(Test::Unit::AutoRunner.run(true, test_dir))
