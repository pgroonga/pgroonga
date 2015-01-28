# -*- ruby -*-

package = "pgroonga"

def find_version(package)
  control_content = File.read("#{package}.control")
  if /^default_version\s*=\s*'(.+)'$/ =~ control_content
    $1
  else
    nil
  end
end

version = find_version(package)

archive_name = "#{package}-#{version}.tar.gz"

dist_files = `git ls-files`.split("\n")

file archive_name => dist_files do
  sh("git archive --format=tar HEAD | gzip > #{archive_name}")
end

desc "Create release package"
task :dist => archive_name
