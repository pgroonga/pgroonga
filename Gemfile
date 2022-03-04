# -*- ruby -*-

source "https://rubygems.org"

gem "archive-zip"
gem "mime-types"
gem "octokit"
gem "rake"
gem "veyor"

pgroonga_benchmark_gemfile = ENV["PGROONGA_BENCHMARK_GEMFILE"]
if pgroonga_benchmark_gemfile
  eval_gemfile pgroonga_benchmark_gemfile
end
