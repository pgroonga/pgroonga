#!/usr/bin/env ruby

require "English"
require "nkf"
require "json"

def escape(value)
  case value
  when String
    escaped_value = value.gsub(/'/, "''")
    "'#{escaped_value}'"
  when Array
    escaped_value = value.collect {|element| escape(element)}
    "ARRAY[#{escaped_value.join(', ')}]"
  end
end

current_term = nil
current_readings = []
current_english = nil

first_value = true

flush = lambda do
  return if current_term.nil?

  if first_value
    first_value = false
    puts
  else
    puts ","
  end
  print <<-VALUE.chomp
(#{escape(current_term)},
 #{escape(current_readings)},
 #{escape(current_english)})
  VALUE
  current_term = nil
end

_first_line = gets # Ignore header

print "INSERT INTO dictionary VALUES"
loop do
  raw_line = gets
  break if raw_line.nil?

  line = raw_line.encode("UTF-8", "EUC-JP")
  term, english = line.strip.split("/", 2)
  term = term.strip
  if /\s*\[(.+)\]\z/ =~ term
    term = $PREMATCH
    reading = $1
    reading = NKF.nkf("-Ww --katakana", reading)
  else
    reading = NKF.nkf("-Ww --katakana", term)
  end

  case current_term
  when nil
    current_term = term
    current_readings = [reading]
    current_english = english
  when term
    current_readings << reading
    unless english == current_english
      current_english << english
    end
  else
    flush.call
  end
end
flush.call
puts ";"
