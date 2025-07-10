#!/usr/bin/env ruby

require "find"
require "fileutils"

def open_output(output_path)
  if output_path
    File.open(output_path, "w") do |output|
      yield(output)
    end
  else
    yield($stdout)
  end
end

open_output(ARGV[0]) do |output|
  Find.find("sql") do |entry|
    if File.directory?(entry)
      results_directory = entry.gsub(/\Asql/, "results")
      build_dir = ENV["BUILD_DIR"]
      if build_dir
        results_directory = File.join(build_dir, results_directory)
      end
      FileUtils.mkdir_p(results_directory)
    elsif File.file?(entry)
      next unless entry.end_with?(".sql")
      test_file = entry.gsub(/\Asql\/|\.sql\z/, "")
      output.puts("test: #{test_file}")
    end
  end
end
