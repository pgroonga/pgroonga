#!/usr/bin/env ruby

require "test-unit"

test_dir = __dir__
exit(Test::Unit::AutoRunner.run(true, test_dir))
