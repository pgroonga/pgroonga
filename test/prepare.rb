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
  source_dir = File.dirname(File.dirname(__FILE__))
  sql_dir = File.join(source_dir, "sql")
  Find.find(sql_dir) do |entry|
    if File.directory?(entry)
      results_directory = entry.gsub(/\A#{sql_dir}/, "results")
      build_dir = ENV["BUILD_DIR"]
      if build_dir
        results_directory = File.join(build_dir, results_directory)
      end
      FileUtils.mkdir_p(results_directory)
    elsif File.file?(entry)
      next unless entry.end_with?(".sql")
      test_file = entry.gsub(/\A#{sql_dir}\/|\.sql\z/, "")
      output.puts("test: #{test_file}")
    end
  end
end
