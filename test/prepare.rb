#!/usr/bin/env ruby

require "find"
require "fileutils"

Find.find("sql") do |entry|
  if File.directory?(entry)
    results_directory = entry.gsub(/\Asql/, "results")
    FileUtils.mkdir_p(results_directory)
  elsif File.file?(entry)
    next unless entry.end_with?(".sql")
    test_file = entry.gsub(/sql\z/, "test")
    puts(test_file)
  end
end
