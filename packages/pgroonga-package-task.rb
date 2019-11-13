require "pathname"
require "time"
require_relative "../helper"

apache_arrow_repository = ENV["APACHE_ARROW_REPOSITORY"]
if apache_arrow_repository.nil?
  raise "Specify APACHE_ARROW_REPOSITORY environment variable"
end
require "#{apache_arrow_repository}/dev/tasks/linux-packages/package-task"

class PGroongaPackageTask < PackageTask
  def initialize(package)
    super(package, Helper.detect_version("pgroonga"), detect_release_time)
    @original_archive_base_name = "pgroonga-#{@version}"
    @original_archive_name = "#{@original_archive_base_name}.tar.gz"
  end

  private
  def detect_release_time
    release_time_env = ENV["RELEASE_TIME"] || ENV["NEW_RELEASE_DATE"]
    if release_time_env
      Time.parse(release_time_env).utc
    else
      Time.now.utc
    end
  end

  def latest_groonga_version
    @latest_groonga_version ||= Helper.detect_latest_groonga_version
  end

  def top_directory
    packages_directory.parent
  end

  def packages_directory
    Pathname(__dir__)
  end

  def package_directory
    packages_directory + @package
  end

  def original_archive_path
    top_directory + @original_archive_name
  end

  def rpm_archive_name
    @original_archive_name
  end

  def define_archive_task
    [@archive_name, deb_archive_name, rpm_archive_name].each do |archive_name|
      file archive_name => original_archive_path.to_s do
        sh("tar", "xf", original_archive_path.to_s)
        archive_base_name = File.basename(archive_name, ".tar.gz")
        if @original_archive_base_name != archive_base_name
          mv(@original_archive_base_name, archive_base_name)
        end
        sh("tar", "czf", archive_name, archive_base_name)
        rm_r(archive_base_name)
      end
    end
  end
end

class PGroongaAptPackageTask < PGroongaPackageTask
  def initialize(postgresql_version)
    super("postgresql-#{postgresql_version}-pgroonga")
  end

  def define
    super
    define_debian_control_task
  end

  private
  def define_debian_control_task
    control_paths = []
    debian_directory = package_directory + "debian"
    control_in_path = debian_directory + "control.in"
    apt_targets.each do |target|
      distribution, code_name, _architecture = target.split("-", 3)
      target_debian_directory = package_directory + "debian.#{target}"
      control_path = target_debian_directory + "control"
      control_paths << control_path.to_s
      file control_path.to_s => control_in_path.to_s do |task|
        control_in_content = control_in_path.read
        control_content =
          control_in_content
            .gsub(/@GROONGA_VERSION@/,
                  latest_groonga_version)
        rm_rf(target_debian_directory)
        cp_r(debian_directory, target_debian_directory)
        control_path.open("w") do |file|
          file.puts(control_content)
        end
      end
    end
    namespace :apt do
      task :build => control_paths
    end
  end

  def enable_yum?
    false
  end
end

class PGroongaYumPackageTask < PGroongaPackageTask
  def initialize(postgresql_version)
    @postgresql_version = postgresql_version
    @postgresql_package_version = postgresql_version.gsub(".", "")
    super("postgresql#{@postgresql_package_version}-pgroonga")
  end

  def define
    super
    define_yum_spec_in_task
  end

  private
  def enable_apt?
    false
  end

  def yum_expand_variable(key)
    case key
    when "PG_VERSION"
      @postgresql_version
    when "PG_PACKAGE_VERSION"
      @postgresql_package_version
    when "GROONGA_VERSION"
      latest_groonga_version
    else
      super
    end
  end

  def source_yum_spec_in_path
    packages_directory + "yum" + "postgresql-pgroonga.spec.in"
  end

  def define_yum_spec_in_task
    file yum_spec_in_path => source_yum_spec_in_path do
      mkdir_p(File.dirname(yum_spec_in_path))
      cp(source_yum_spec_in_path,
         yum_spec_in_path)
    end
  end

  def update_spec
    super
    cp(yum_spec_in_path, source_yum_spec_in_path)
  end
end
