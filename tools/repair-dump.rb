##!/usr/bin/env ruby

input_path = ARGV[0]

# INDEX => SCHEMA.INDEX
# SCHEMA.INDEX => SCHEMA.INDEX
referred_targets = {}
# Collect referred PGroonga indexes
File.open(input_path) do |input|
  input.each_line do |line|
    line.scan(/\${table:(.+?)}/) do |referred_index, |
      referred_index_no_schema = referred_index.gsub(/\A[^.]+\./, "")
      if referred_index == referred_index_no_schema
        referred_targets[referred_index] ||= referred_index
      else
        referred_targets[referred_index] = referred_index
        referred_targets[referred_index_no_schema] = referred_index
      end
    end
  end
end
# Complement schema of collected indexes
File.open(input_path) do |input|
  target_patterns = referred_targets.keys.collect do |index|
    if index.include?(".")
      Regexp.escape(index)
    else
      "(?:[^ .]+?\\.)?" + Regexp.escape(index)
    end
  end
  pattern = /\ACREATE INDEX ((?:#{target_patterns.join("|")})) ON (.+?) /
  input.each_line do |line|
    line.scan(pattern) do |index, table, |
      next if index.include?(".")
      next unless table.include?(".")
      # Complement schema
      schema, = table.split(".", 2)
      index_with_schema = "#{schema}.#{index}"
      referred_targets[index] = index_with_schema
      referred_targets[index_with_schema] = index_with_schema
    end
  end
end

def normalize_table(line, referred_targets)
  line.gsub(/\${table:(.+?)}/) do
    target = $1
    target_with_schema = referred_targets[target] || target
    "${table:#{target_with_schema}}"
  end
end

File.open(input_path) do |input|
  input.each_line do |line|
    print(normalize_table(line, referred_targets))
  end
end
