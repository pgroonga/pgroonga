# -*- ruby -*-

require "open-uri"
require "octokit"

latest_groonga_version = "5.1.1"
windows_postgresql_version = "9.4.5-1"

package = "pgroonga"
rsync_base_path = "packages@packages.groonga.org:public"
gpg_uid = "45499429"
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
  control_content = File.read(control_file(package))
  if /^default_version\s*=\s*'(.+)'$/ =~ control_content
    $1
  else
    nil
  end
end

def env_value(name)
  value = ENV[name]
  raise "Specify #{name} environment variable" if value.nil?
  value
end

def download(url, download_dir)
  base_name = url.split("/").last
  absolute_output_path = File.join(download_dir, base_name)

  unless File.exist?(absolute_output_path)
    mkdir_p(download_dir)
    rake_output_message "Downloading... #{url}"
    open(url) do |downloaded_file|
      File.open(absolute_output_path, "wb") do |output_file|
        output_file.print(downloaded_file.read)
      end
    end
  end

  absolute_output_path
end

def extract_zip(filename, destrination_dir)
  require "archive/zip"

  Archive::Zip.extract(filename, destrination_dir)
end

def export_source(base_name)
  sh("git archive --prefix=#{base_name}/ --format=tar HEAD | " +
     "tar xf -")
  sh("(cd vendor/xxHash && " +
     "git archive --prefix=#{base_name}/vendor/xxHash/ --format=tar HEAD) | " +
     "tar xf -")
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
       "http://packages.groonga.org/tmp/#{groonga_archive_name}")
  else
    groonga_archive_name = "#{groonga_base_name}.zip"
    sh("curl",
       "-O",
       "http://packages.groonga.org/source/groonga/#{groonga_archive_name}")
  end
  rm_rf(groonga_base_name)
  sh("unzip", groonga_archive_name)
  rm(groonga_archive_name)
  cd("#{groonga_base_name}/vendor") do
    ruby("download_mecab.rb")
  end
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
     "-m", "#{package} #{version} has been released!!!")
  sh("git", "push", "--tags")
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

  namespace :yum do
    distribution = "centos"
    rpm_package = "postgresql94-#{package}"
    rsync_path = rsync_base_path
    yum_dir = "#{packages_dir}/yum"
    repositories_dir = "#{yum_dir}/repositories"

    directory repositories_dir

    env_sh = "#{yum_dir}/env.sh"
    file env_sh => __FILE__ do
      File.open(env_sh, "w") do |file|
        file.puts(<<-ENV)
SOURCE_ARCHIVE=#{archive_name}
PACKAGE=#{rpm_package}
VERSION=#{version}
DEPENDED_PACKAGES="
gcc
make
pkg-config
groonga-devel
postgresql94-devel
"
        ENV
      end
    end

    spec = "#{yum_dir}/#{rpm_package}.spec"
    spec_in = "#{spec}.in"
    file spec => [spec_in, control_file(package)] do
      spec_in_data = File.read(spec_in)
      spec_data = spec_in_data.gsub(/@(.+)@/) do |matched|
        case $1
        when "PACKAGE"
          rpm_package
        when "VERSION"
          version
        else
          matched
        end
      end
      File.open(spec, "w") do |spec_file|
        spec_file.print(spec_data)
      end
    end

    desc "Build RPM packages"
    task :build => [archive_name, env_sh, spec, repositories_dir] do
      tmp_dir = "#{yum_dir}/tmp"
      rm_rf(tmp_dir)
      mkdir_p(tmp_dir)
      cp(archive_name, tmp_dir)
      tmp_distribution_dir = "#{tmp_dir}/#{distribution}"
      mkdir_p(tmp_distribution_dir)
      cp(spec, tmp_distribution_dir)

      cd(yum_dir) do
        sh("vagrant", "destroy", "--force")
        distribution_versions = {
          "5" => ["i386", "x86_64"],
          "6" => ["i386", "x86_64"],
          "7" => ["x86_64"],
        }
        distribution_versions.each do |ver, archs|
          archs.each do |arch|
            id = "#{distribution}-#{ver}-#{arch}"
            sh("vagrant", "up", id)
            sh("vagrant", "destroy", "--force", id)
          end
        end
      end
    end

    desc "Sign packages"
    task :sign do
      sh("#{groonga_source_dir}/packages/yum/sign-rpm.sh",
         gpg_uid,
         "#{repositories_dir}/",
         distribution)
    end

    desc "Update repositories"
    task :update do
      sh("#{groonga_source_dir}/packages/yum/update-repository.sh",
         "groonga",
         "#{repositories_dir}/",
         distribution)
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
    code_names = [
      "jessie",
    ]
    architectures = [
      "i386",
      "amd64",
    ]
    rsync_path = rsync_base_path
    debian_dir = "#{packages_dir}/debian94"
    apt_dir = "#{packages_dir}/apt"
    repositories_dir = "#{apt_dir}/repositories"

    directory repositories_dir

    env_sh = "#{apt_dir}/env.sh"
    file env_sh => __FILE__ do
      File.open(env_sh, "w") do |file|
        file.puts(<<-ENV)
PACKAGE=#{package}
VERSION=#{version}
DEPENDED_PACKAGES="
debhelper
pkg-config
libgroonga-dev
postgresql-server-dev-9.4
"
        ENV
      end
    end

    desc "Build DEB packages"
    task :build => [archive_name, env_sh, repositories_dir] do
      tmp_dir = "#{apt_dir}/tmp"
      rm_rf(tmp_dir)
      mkdir_p(tmp_dir)
      cp(archive_name, tmp_dir)
      cp_r(debian_dir, "#{tmp_dir}/debian")

      cd(apt_dir) do
        sh("vagrant", "destroy", "--force")
        code_names.each do |code_name|
          architectures.each do |arch|
            id = "#{distribution}-#{code_name}-#{arch}"
            sh("vagrant", "up", id)
            sh("vagrant", "destroy", "--force", id)
          end
        end
      end
    end

    desc "Sign packages"
    task :sign do
      sh("#{groonga_source_dir}/packages/apt/sign-packages.sh",
         gpg_uid,
         "#{repositories_dir}/",
         code_names.join(" "))
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
        sh("#{groonga_source_dir}/packages/apt/sign-repository.sh",
           gpg_uid,
           "#{repositories_dir}/",
           code_names.join(" "))
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
      desc "Upload package for PostgreSQL 9.3"
      task :postgresql93 => [archive_name] do
        ruby("#{groonga_source_dir}/packages/ubuntu/upload.rb",
             "--package", package,
             "--version", version,
             "--source-archive", archive_name,
             "--code-names", "trusty",
             "--debian-directory", "packages/debian93",
             "--pgp-sign-key", env_value("LAUNCHPAD_UPLOADER_PGP_KEY"))
      end

      desc "Upload package for PostgreSQL 9.4"
      task :postgresql94 => [archive_name] do
        ruby("#{groonga_source_dir}/packages/ubuntu/upload.rb",
             "--package", package,
             "--version", version,
             "--source-archive", archive_name,
             "--code-names", "vivid,wily",
             "--debian-directory", "packages/debian94",
             "--pgp-sign-key", env_value("LAUNCHPAD_UPLOADER_PGP_KEY"))
      end
    end

    desc "Upload package"
    upload_tasks = [
      "package:ubuntu:upload:postgresql93",
      "package:ubuntu:upload:postgresql94",
    ]
    task :upload => upload_tasks
  end

  namespace :windows do
    windows_packages_dir = "#{packages_dir}/windows"
    rsync_path = "#{rsync_base_path}/windows/pgroonga"
    windows_postgresql_download_base = "http://get.enterprisedb.com/postgresql"

    directory windows_packages_dir

    windows_architectures = ["x86", "x64"]
    windows_packages = []
    windows_architectures.each do |arch|
      windows_package =
        "pgroonga-#{version}-postgresql-#{windows_postgresql_version}-#{arch}.zip"
      windows_packages << windows_package
      file windows_package => windows_packages_dir do
        rm_rf("tmp")
        mkdir_p("tmp")
        cd("tmp") do
          cmake_generator = "Visual Studio 12 2013"
          if arch == "x64"
            cmake_generator << " Win64"
            windows_postgresql_archive_name =
              "postgresql-#{windows_postgresql_version}-windows-x64-binaries.zip"
          else
            windows_postgresql_archive_name =
              "postgresql-#{windows_postgresql_version}-windows-binaries.zip"
          end
          windows_postgresql_url =
            "#{windows_postgresql_download_base}/#{windows_postgresql_archive_name}"
          download(windows_postgresql_url, ".")
          extract_zip(windows_postgresql_archive_name, ".")

          windows_pgroonga_source_name_base = "#{package}-#{version}"
          windows_pgroonga_source_name =
            "#{windows_pgroonga_source_name_base}.zip"
          windows_pgroonga_source_url_base =
            "http://packages.groonga.org/source/#{package}"
          windows_pgroonga_source_url =
            "#{windows_pgroonga_source_url_base}/#{windows_pgroonga_source_name}"
          download(windows_pgroonga_source_url, ".")
          extract_zip(windows_pgroonga_source_name, ".")

          sh("cmake",
             windows_pgroonga_source_name_base,
             "-G", cmake_generator,
             "-DCMAKE_INSTALL_PREFIX=pgsql",
             "-DPGRN_POSTGRESQL_VERSION=#{windows_postgresql_version}",
             "-DGRN_WITH_BUNDLED_MECAB=yes")
          sh("cmake",
             "--build", ".",
             "--config", "Release")
          sh("cmake",
             "--build", ".",
             "--config", "Release",
             "--target", "package")
          mv(windows_package, "..")
        end
      end
    end

    desc "Build packages"
    task :build => windows_packages

    desc "Upload packages"
    task :upload => windows_packages do
      download("http://curl.haxx.se/ca/cacert.pem", ".")
      ENV["SSL_CERT_FILE"] ||= File.expand_path("cacert.pem")

      pgroonga_repository = "pgroonga/pgroonga"
      tag_name = version

      client = Octokit::Client.new(:access_token => env_value("GITHUB_TOKEN"))

      releases = client.releases(pgroonga_repository)
      current_release = releases.find do |release|
        release.tag_name == tag_name
      end
      current_release ||= client.create_release(pgroonga_repository, tag_name)

      options = {
        :content_type => "application/zip",
      }
      windows_packages.each do |windows_package|
        client.upload_asset(current_release.url, windows_package, options)
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
           "packages/debian93/changelog",
           "packages/debian94/changelog",
           "packages/yum/postgresql94-pgroonga.spec.in")
    end
  end
end
