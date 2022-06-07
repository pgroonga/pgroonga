# -*- ruby -*-

require "open-uri"
require_relative "helper"

package = "pgroonga"
package_label = "PGroonga"
rsync_base_path = "packages@packages.groonga.org:public"

def latest_groonga_version
  @latest_groonga_version ||= Helper.detect_latest_groonga_version
end

def export_source(base_name)
  sh("git archive --prefix=#{base_name}/ --format=tar HEAD | " +
     "tar xf -")
  sh("(cd vendor/xxHash && " +
     "git archive --prefix=#{base_name}/vendor/xxHash/ --format=tar HEAD) | " +
     "tar xf -")
end

version = Helper.detect_version(package)

archive_base_name = "#{package}-#{version}"
suffix = ENV["SUFFIX"]
if suffix
  archive_base_name << suffix
end
archive_name = "#{archive_base_name}.tar.gz"
windows_archive_name = "#{archive_base_name}.zip"

dist_files = `git ls-files`.split("\n").reject do |file|
  file.start_with?("packages/")
end

file archive_name => dist_files do
  export_source(archive_base_name)
  sh("tar", "czf", archive_name, archive_base_name)
  rm_r(archive_base_name)
end

file windows_archive_name => dist_files do
  export_source(archive_base_name)
  groonga_base_name = "groonga-#{latest_groonga_version}"
  groonga_suffix = ENV["GROONGA_SUFFIX"]
  if groonga_suffix
    groonga_base_name << groonga_suffix
    groonga_archive_name = "#{groonga_base_name}.zip"
    sh("curl",
       "-O",
       "https://packages.groonga.org/tmp/#{groonga_archive_name}")
  else
    groonga_archive_name = "#{groonga_base_name}.zip"
    sh("curl",
       "-O",
       "https://packages.groonga.org/source/groonga/#{groonga_archive_name}")
  end
  rm_rf(groonga_base_name)
  sh("unzip", groonga_archive_name)
  rm(groonga_archive_name)
  mkdir_p("#{archive_base_name}/vendor")
  mv(groonga_base_name, "#{archive_base_name}/vendor/groonga")
  rm_f(windows_archive_name)
  sh("zip", "-r", windows_archive_name, archive_base_name)
  rm_r(archive_base_name)
end

desc "Create release package"
task :dist => [archive_name, windows_archive_name]

desc "Tag #{version}"
task :tag do
  sh("git", "tag",
     "-a", version,
     "-m", "#{package_label} #{version} has been released!!!")
  sh("git", "push", "--tags")
end

namespace :version do
  desc "Update version"
  task :update do
    new_version = Helper.env_value("NEW_VERSION")

    Dir.glob("*.control") do |control_path|
      package_name = File.basename(control_path, ".*")
      content = File.read(control_path)
      content = content.gsub(/^default_version = '.+?'/,
                             "default_version = '#{new_version}'")
      File.open(control_path, "w") do |control_file|
        control_file.print(content)
      end
      sh("git", "add", control_path)

      upgrade_sql_name = "#{package_name}--#{version}--#{new_version}.sql"
      upgrade_sql_path = File.join("data", upgrade_sql_name)
      File.open(upgrade_sql_path, "w") do |upgrade_sql|
        upgrade_sql.puts("-- Upgrade SQL")
      end
      sh("git", "add", upgrade_sql_path)
    end
  end
end

packages_dir = "packages"

namespace :package do
  package_names = [
    "pgroonga",
    "postgresql-10-pgroonga",
    "postgresql-10-pgdg-pgroonga",
    "postgresql-11-pgroonga",
    "postgresql-12-pgroonga",
    "postgresql-12-pgdg-pgroonga",
    "postgresql-13-pgroonga",
    "postgresql-13-pgdg-pgroonga",
    "postgresql-14-pgroonga",
    "postgresql-14-pgdg-pgroonga",
  ]

  namespace :source do
    rsync_path = "#{rsync_base_path}/source/#{package}"
    source_dir = "#{packages_dir}/source"

    directory source_dir

    desc "Clean sources"
    task :clean do
      rm_rf(source_dir)
    end

    desc "Upload sources"
    task :upload => [archive_name, windows_archive_name, source_dir] do
      groonga_repository = ENV["GROONGA_REPOSITORY"]
      if groonga_repository.nil?
        raise "Specify GROONGA_REPOSITORY environment variable"
      end
      gpg_uid = File.read(File.join(groonga_repository, "gpg_uid_rsa4096")).strip

      prepare_archive = lambda do |archive, latest_archive|
        cp(archive, source_dir)
        cd(source_dir) do
          ln_sf(archive, latest_archive)
          sh("gpg",
             "--local-user", gpg_uid,
             "--armor",
             "--detach-sign",
             archive)
          ln_sf("#{archive}.asc", "#{latest_archive}.asc")
        end
      end

      prepare_archive.call(archive_name, "#{package}-latest.tar.gz")
      prepare_archive.call(windows_archive_name, "#{package}-latest.zip")
      sh("rsync", "-avz", "--progress", "#{source_dir}/", rsync_path)
    end

    namespace :snapshot do
      desc "Upload snapshot sources"
      task :upload => [archive_name, windows_archive_name] do
        sh("scp", archive_name, "#{rsync_base_path}/tmp")
        sh("scp", windows_archive_name, "#{rsync_base_path}/tmp")
      end
    end
  end

  desc "Release sources"
  source_tasks = [
    "package:source:clean",
    "package:source:upload",
  ]
  task :source => source_tasks

  desc "Release APT packages"
  task :apt do
    package_names.each do |package_name|
      cd("packages/#{package_name}") do
        ruby("-S", "rake", "apt")
      end
    end
  end

  desc "Release Ubuntu packages"
  task :ubuntu do
    package_names.each do |package_name|
      cd("packages/#{package_name}") do
        ruby("-S", "rake", "ubuntu", "--trace")
      end
    end
  end

  desc "Release Yum packages"
  task :yum do
    package_names.each do |package_name|
      cd("packages/#{package_name}") do
        ruby("-S", "rake", "yum")
      end
    end
  end

  namespace :version do
    desc "Update versions"
    task :update do
      package_names.each do |package_name|
        cd("packages/#{package_name}") do
          ruby("-S", "rake", "version:update")
        end
      end
      spec_in = nil
      package_names.reverse_each do |package_name|
        spec_in = Dir.glob("packages/#{package_name}/yum/*.spec.in").first
        break if spec_in
      end
      cp(spec_in, "packages/yum/postgresql-pgroonga.spec.in")
    end
  end
end
