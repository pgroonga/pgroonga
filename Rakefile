# -*- ruby -*-

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

def find_version(package)
  control_content = File.read("#{package}.control")
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

version = find_version(package)

archive_base_name = "#{package}-#{version}"
archive_name = "#{archive_base_name}.tar.gz"

dist_files = `git ls-files`.split("\n").reject do |file|
  file.start_with?("packages/")
end

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

packages_dir = "packages"

namespace :package do
  namespace :source do
    rsync_path = "#{rsync_base_path}/source/#{package}"
    source_dir = "#{packages_dir}/source"

    directory source_dir

    desc "Download sources"
    task :download => source_dir do
      sh("rsync", "-avz", "--progress", "#{rsync_path}/", source_dir)
    end

    desc "Upload sources"
    task :upload => [archive_name, source_dir] do
      cp(archive_name, source_dir)
      cd(source_dir) do
        ln_sf(archive_name, "#{package}-latest.tar.gz")
      end
      sh("rsync", "-avz", "--progress", "--delete", "#{source_dir}/", rsync_path)
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
    file spec => spec_in do
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

  namespace :ubuntu do
    desc "Upload package"
    task :upload do
      ruby("#{groonga_source_dir}/packages/ubuntu/upload.rb",
           "--package", package,
           "--version", version,
           "--source-archive", archive_name,
           "--code-names", "utopic",
           "--debian-directory", "packages/debian",
           "--pgp-sign-key", env_value("LAUNCHPAD_UPLOADER_PGP_KEY"))
    end
  end

  namespace :version do
    desc "Update versions"
    task :update do
      ruby("#{groonga_source_dir}/misc/update-latest-release.rb",
           package,
           env_value("OLD_RELEASE"),
           env_value("OLD_RELEASE_DATE"),
           version,
           env_value("NEW_RELEASE_DATE"),
           "README.md",
           "packages/debian/changelog",
           "packages/yum/postgresql94-pgroonga.spec.in")
    end
  end
end
