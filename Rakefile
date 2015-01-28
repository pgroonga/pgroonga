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

archive_base_name = "#{package}-#{version}"
archive_name = "#{archive_base_name}.tar.gz"

dist_files = `git ls-files`.split("\n")

file archive_name => dist_files do
  sh("git archive --prefix=#{archive_base_name}/ --format=tar HEAD | " +
     "gzip > #{archive_name}")
end

desc "Create release package"
task :dist => archive_name

desc "Tag #{version}"
task :tag do
  sh("git", "tag",
     "-a", version,
     "-m", "#{package} #{version} has been released!!!")
  sh("git", "push", "--tags")
end
