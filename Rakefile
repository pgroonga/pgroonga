# -*- ruby -*-

require "octokit"
require "open-uri"
require "veyor"

package = "pgroonga"
package_label = "PGroonga"
rsync_base_path = "packages@packages.groonga.org:public"
gpg_uids = [
  "C97E4649A2051D0CEA1A73F972A7496B45499429",
#  "2701F317CFCCCB975CADE9C2624CF77434839225",
]
groonga_source_dir_candidates = [
  "../groonga.clean",
  "../groonga",
]
groonga_source_dir = groonga_source_dir_candidates.find do |candidate|
  File.exist?(candidate)
end
groonga_source_dir = File.expand_path(groonga_source_dir) if groonga_source_dir
cutter_source_dir = File.expand_path("../cutter")

def control_file(package)
  "#{package}.control"
end

def find_version(package)
  env_version = ENV["VERSION"]
  return env_version if env_version

  control_content = File.read(control_file(package))
  if /^default_version\s*=\s*'(.+)'$/ =~ control_content
    $1
  else
    nil
  end
end

def latest_groonga_version
  @latest_groonga_version ||= detect_latest_groonga_version
end

def detect_latest_groonga_version
  open("https://packages.groonga.org/source/groonga/") do |groonga_sources|
    versions = groonga_sources.read.scan(/<a href="groonga-([\d.]+).zip">/)
    versions.flatten.sort.last
  end
end

def env_value(name)
  value = ENV[name]
  raise "Specify #{name} environment variable" if value.nil?
  value
end

def export_source(base_name)
  sh("git archive --prefix=#{base_name}/ --format=tar HEAD | " +
     "tar xf -")
  sh("(cd vendor/xxHash && " +
     "git archive --prefix=#{base_name}/vendor/xxHash/ --format=tar HEAD) | " +
     "tar xf -")
end

def prepare_debian_dir(source, destination, variables)
  cp_r(source, destination)
  control_path = "#{destination}/control"
  control_in_path = "#{control_path}.in"
  control_in_data = File.read(control_in_path)
  control_data = control_in_data.gsub(/@(.+?)@/) do |matched|
    variables[$1] || matched
  end
  File.open(control_path, "w") do |control|
    control.print(control_data)
  end
  rm_f(control_in_path)
end

def debian_variables
  {
    "GROONGA_VERSION" => latest_groonga_version,
  }
end

version = find_version(package)

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
    current_version = find_version(package)
    new_version = env_value("NEW_VERSION")

    Dir.glob("*.control") do |control_path|
      package_name = File.basename(control_path, ".*")
      content = File.read(control_path)
      content = content.gsub(/^default_version = '.+?'/,
                             "default_version = '#{new_version}'")
      File.open(control_path, "w") do |control_file|
        control_file.print(content)
      end

      touch(File.join("data",
                      "#{package_name}--#{current_version}--#{new_version}.sql"))
    end
  end
end

packages_dir = "packages"

namespace :package do
  namespace :source do
    rsync_path = "#{rsync_base_path}/source/#{package}"
    source_dir = "#{packages_dir}/source"

    directory source_dir

    desc "Download sources"
    task :download => source_dir do
      sh("rsync", "-avz", "--progress", "--delete", "#{rsync_path}/", source_dir)
    end

    desc "Upload sources"
    task :upload => [archive_name, windows_archive_name, source_dir] do
      cp(archive_name, source_dir)
      cd(source_dir) do
        ln_sf(archive_name, "#{package}-latest.tar.gz")
      end
      cp(windows_archive_name, source_dir)
      cd(source_dir) do
        ln_sf(windows_archive_name, "#{package}-latest.zip")
      end
      sh("rsync", "-avz", "--progress", "--delete", "#{source_dir}/", rsync_path)
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
    "package:source:download",
    "package:source:upload",
  ]
  task :source => source_tasks


  namespace :yum do
    distribution = "centos"
    rsync_path = rsync_base_path
    yum_dir = "#{packages_dir}/yum"
    repositories_dir = "#{yum_dir}/repositories"

    directory repositories_dir

    postgresql_package_versions = [
      "94",
      "95",
      "96",
      "10",
      "11",
    ]
    namespace :build do
      postgresql_package_versions.each do |postgresql_package_version|
        rpm_package = "postgresql#{postgresql_package_version}-#{package}"

        if postgresql_package_version.start_with?("9")
          postgresql_version = postgresql_package_version.scan(/(.)/).join(".")
        else
          postgresql_version = postgresql_package_version
        end
        desc "Build RPM packages for PostgreSQL #{postgresql_version}"
        task postgresql_package_version => [archive_name, repositories_dir] do
          tmp_dir = "#{yum_dir}/tmp"
          rm_rf(tmp_dir)
          mkdir_p(tmp_dir)
          cp(archive_name, tmp_dir)

          env_sh = "#{yum_dir}/env.sh"
          File.open(env_sh, "w") do |file|
            if postgresql_version == "11"
              llvm_package_names = ["llvm-toolset-7", "llvm5.0-devel"]
            else
              llvm_package_names = []
            end
            file.puts(<<-ENV)
SOURCE_ARCHIVE=#{archive_name}
PACKAGE=#{rpm_package}
VERSION=#{version}
PG_VERSION=#{postgresql_version}
PG_PACKAGE_VERSION=#{postgresql_package_version}
DEPENDED_PACKAGES="
gcc
make
pkg-config
groonga-devel
msgpack-devel
postgresql#{postgresql_package_version}-devel
#{llvm_package_names.join("\n")}
"
            ENV
          end

          tmp_distribution_dir = "#{tmp_dir}/#{distribution}"
          mkdir_p(tmp_distribution_dir)
          spec = "#{tmp_distribution_dir}/#{rpm_package}.spec"
          spec_in = "#{yum_dir}/postgresql-pgroonga.spec.in"
          spec_in_data = File.read(spec_in)
          spec_data = spec_in_data.gsub(/@(.+?)@/) do |matched|
            case $1
            when "PG_VERSION"
              postgresql_version
            when "PG_PACKAGE_VERSION"
              postgresql_package_version
            when "PACKAGE"
              rpm_package
            when "VERSION"
              version
            when "GROONGA_VERSION"
              latest_groonga_version
            else
              matched
            end
          end
          File.open(spec, "w") do |spec_file|
            spec_file.print(spec_data)
          end

          cd(yum_dir) do
            sh("vagrant", "destroy", "--force")
            distribution_versions = {
              "6" => ["i386", "x86_64"],
              "7" => ["x86_64"],
            }
            distribution_versions.each do |ver, archs|
              next if postgresql_package_version == "96" and ver == "5"
              archs.each do |arch|
                id = "#{distribution}-#{ver}-#{arch}"
                sh("vagrant", "up", id)
                sh("vagrant", "destroy", "--force", id)
              end
            end
          end
        end
      end
    end

    desc "Build RPM packages"
    build_tasks = postgresql_package_versions.collect do |package_version|
      "build:#{package_version}"
    end
    task :build => build_tasks

    desc "Sign packages"
    task :sign do
      gpg_uids.each do |gpg_uid|
        sh("#{groonga_source_dir}/packages/yum/sign-rpm.sh",
           gpg_uid,
           "#{repositories_dir}/",
           distribution)
      end
    end

    desc "Update repositories"
    task :update do
      gpg_uids.each do |gpg_uid|
        sh("#{groonga_source_dir}/packages/yum/update-repository.sh",
           gpg_uid,
           "groonga",
           "#{repositories_dir}/",
           distribution)
      end
    end

    desc "Download repositories"
    task :download => repositories_dir do
      sh("rsync", "-avz", "--progress",
         "--delete",
         "#{rsync_path}/#{distribution}/",
         "#{repositories_dir}/#{distribution}")
    end

    desc "Upload repositories"
    task :upload => repositories_dir do
      sh("rsync", "-avz", "--progress",
         "--delete",
         "#{repositories_dir}/#{distribution}/",
         "#{rsync_path}/#{distribution}")
    end
  end

  desc "Release Yum packages"
  yum_tasks = [
    "package:yum:download",
    "package:yum:build",
    "package:yum:sign",
    "package:yum:update",
    "package:yum:upload",
  ]
  task :yum => yum_tasks

  namespace :apt do
    distribution = "debian"
    architectures = [
      "i386",
      "amd64",
    ]
    targets = {
      "stretch" => [
        "9.6-system",
        "10",
        "11",
      ],
      "buster" => [
        "11-system",
      ],
    }
    code_names = targets.keys
    rsync_path = rsync_base_path
    apt_dir = "#{packages_dir}/apt"
    repositories_dir = "#{apt_dir}/repositories"

    directory repositories_dir

    desc "Build DEB packages"
    task :build => [archive_name, repositories_dir] do
      tmp_dir = "#{apt_dir}/tmp"
      rm_rf(tmp_dir)
      mkdir_p(tmp_dir)
      cp(archive_name, tmp_dir)
      absolute_packages_dir = File.expand_path(packages_dir)

      cd(apt_dir) do
        sh("vagrant", "destroy", "--force")
        targets.each do |code_name, postgresql_versions|
          postgresql_versions.each do |postgresql_version|
            architectures.each do |arch|
              use_system_postgresql = postgresql_version.end_with?("-system")
              postgresql_version = postgresql_version.delete_suffix("-system")
              short_postgresql_version = postgresql_version.delete(".")

              rm_rf("tmp/debian")
              debian_dir = "debian#{short_postgresql_version}"
              prepare_debian_dir("#{absolute_packages_dir}/#{debian_dir}",
                                 "tmp/debian",
                                 debian_variables)

              File.open("tmp/env.sh", "w") do |file|
                file.puts(<<-ENV)
PACKAGE=#{package}
VERSION=#{version}
USE_SYSTEM_POSTGRESQL=#{use_system_postgresql}
DEPENDED_PACKAGES="
debhelper
pkg-config
libgroonga-dev
libmsgpack-dev
"
SYSTEM_POSTGRESQL_DEPENDED_PACKAGES="
postgresql-server-dev-#{postgresql_version}
"
OFFICIAL_POSTGRESQL_DEPENDED_PACKAGES="
postgresql-server-dev-#{postgresql_version}
"
                ENV
              end

              id = "#{distribution}-#{code_name}-#{arch}"
              sh("vagrant", "up", id)
              sh("vagrant", "destroy", "--force", id)
            end
          end
        end
      end
    end

    desc "Sign packages"
    task :sign do
      gpg_uids.each do |gpg_uid|
        sh("#{groonga_source_dir}/packages/apt/sign-packages.sh",
           gpg_uid,
           "#{repositories_dir}/",
           code_names.join(" "))
      end
    end

    namespace :repository do
      desc "Update repositories"
      task :update do
        sh("#{groonga_source_dir}/packages/apt/update-repository.sh",
           "Groonga",
           "#{repositories_dir}/",
           architectures.join(" "),
           code_names.join(" "))
      end

      desc "Sign repositories"
      task :sign do
        gpg_uids.each do |gpg_uid|
          sh("#{groonga_source_dir}/packages/apt/sign-repository.sh",
             gpg_uid,
             "#{repositories_dir}/",
             code_names.join(" "))
        end
      end
    end

    desc "Download repositories"
    task :download => repositories_dir do
      sh("rsync", "-avz", "--progress",
         "--delete",
         "#{rsync_path}/#{distribution}/",
         "#{repositories_dir}/#{distribution}")
    end

    desc "Upload repositories"
    task :upload => repositories_dir do
      sh("rsync", "-avz", "--progress",
         "--delete",
         "#{repositories_dir}/#{distribution}/",
         "#{rsync_path}/#{distribution}")
    end
  end

  desc "Release APT packages"
  apt_tasks = [
    "package:apt:download",
    "package:apt:build",
    "package:apt:sign",
    "package:apt:repository:update",
    "package:apt:repository:sign",
    "package:apt:upload",
  ]
  task :apt => apt_tasks

  namespace :ubuntu do
    namespace :upload do
      tmp_dir = "packages/ubuntu/tmp"
      tmp_debian_dir = "#{tmp_dir}/debian"

      desc "Upload package for PostgreSQL 9.5"
      task :postgresql95 => [archive_name] do
        rm_rf(tmp_dir)
        mkdir_p(tmp_dir)
        prepare_debian_dir("packages/debian95",
                           tmp_debian_dir,
                           debian_variables)
        ruby("#{groonga_source_dir}/packages/ubuntu/upload.rb",
             "--package", package,
             "--version", version,
             "--source-archive", archive_name,
             "--ubuntu-code-names", "xenial",
             "--ubuntu-versions", "16.04",
             "--debian-directory", tmp_debian_dir,
             "--pgp-sign-key", env_value("LAUNCHPAD_UPLOADER_PGP_KEY"))
      end

      desc "Upload package for PostgreSQL 10"
      task :postgresql10 => [archive_name] do
        rm_rf(tmp_dir)
        mkdir_p(tmp_dir)
        prepare_debian_dir("packages/debian10",
                           tmp_debian_dir,
                           debian_variables)
        ruby("#{groonga_source_dir}/packages/ubuntu/upload.rb",
             "--package", package,
             "--version", version,
             "--source-archive", archive_name,
             "--ubuntu-code-names", "bionic,cosmic",
             "--ubuntu-versions", "18.04,18.10",
             "--debian-directory", tmp_debian_dir,
             "--pgp-sign-key", env_value("LAUNCHPAD_UPLOADER_PGP_KEY"))
      end

      desc "Upload package for PostgreSQL 11"
      task :postgresql11 => [archive_name] do
        rm_rf(tmp_dir)
        mkdir_p(tmp_dir)
        prepare_debian_dir("packages/debian11",
                           tmp_debian_dir,
                           debian_variables)
        ruby("#{groonga_source_dir}/packages/ubuntu/upload.rb",
             "--package", package,
             "--version", version,
             "--source-archive", archive_name,
             "--ubuntu-code-names", "disco",
             "--ubuntu-versions", "19.04",
             "--debian-directory", tmp_debian_dir,
             "--pgp-sign-key", env_value("LAUNCHPAD_UPLOADER_PGP_KEY"))
      end
    end

    desc "Upload package"
    upload_tasks = [
      "package:ubuntu:upload:postgresql95",
      "package:ubuntu:upload:postgresql10",
      "package:ubuntu:upload:postgresql11",
    ]
    task :upload => upload_tasks
  end

  namespace :windows do
    desc "Upload packages"
    task :upload do
      pgroonga_repository = "pgroonga/pgroonga"
      tag_name = version

      client = Octokit::Client.new(:access_token => env_value("GITHUB_TOKEN"))

      appveyor_url = "https://ci.appveyor.com/"
      appveyor_info = nil
      client.statuses(pgroonga_repository, tag_name).each do |status|
        next unless status.target_url.start_with?(appveyor_url)
        case status.state
        when "success"
          match_data = /\/([^\/]+?)\/([^\/]+?)\/builds\/(\d+)\z/.match(status.target_url)
          appveyor_info = {
            account: match_data[1],
            project: match_data[2],
            build_id: match_data[3],
          }
          break
        when "pending"
          # Ignore
        else
          message = "AppVeyor build isn't succeeded: #{status.state}\n"
          message << "  #{status.target_url}"
          raise message
        end
      end
      if appveyor_info.nil?
        raise "No AppVeyor build"
      end

      releases = client.releases(pgroonga_repository)
      current_release = releases.find do |release|
        release.tag_name == tag_name
      end
      current_release ||= client.create_release(pgroonga_repository, tag_name)

      start_build = appveyor_info[:build_id].to_i + 1
      build_history = Veyor.project_history(account: appveyor_info[:account],
                                            project: appveyor_info[:project],
                                            start_build: start_build,
                                            limit: 1)
      build_version = build_history["builds"][0]["buildNumber"]
      project = Veyor.project(account: appveyor_info[:account],
                              project: appveyor_info[:project],
                              version: build_version)
      project["build"]["jobs"].each do |job|
        job_id = job["jobId"]
        artifacts = Veyor.build_artifacts(job_id: job_id)
        artifacts.each do |artifact|
          file_name = artifact["fileName"]
          url = "#{appveyor_url}api/buildjobs/#{job_id}/artifacts/#{file_name}"
          sh("curl", "--location", "--output", file_name, url)
          options = {
            :content_type => "application/zip",
          }
          client.upload_asset(current_release.url, file_name, options)
        end
      end
    end
  end

  namespace :version do
    desc "Update versions"
    task :update do
      ruby("#{cutter_source_dir}/misc/update-latest-release.rb",
           package,
           env_value("OLD_RELEASE"),
           env_value("OLD_RELEASE_DATE"),
           version,
           env_value("NEW_RELEASE_DATE"),
           "README.md",
           "packages/debian95/changelog",
           "packages/debian96/changelog",
           "packages/debian10/changelog",
           "packages/debian11/changelog",
           "packages/yum/postgresql-pgroonga.spec.in")
    end
  end
end
