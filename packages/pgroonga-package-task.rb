require "pathname"
require "time"
require_relative "../helper"

groonga_repository = ENV["GROONGA_REPOSITORY"]
if groonga_repository.nil?
  raise "Specify GROONGA_REPOSITORY environment variable"
end
require "#{groonga_repository}/packages/packages-groonga-org-package-task"

class GenericPGroongaPackageTask < PackagesGroongaOrgPackageTask
  def initialize(package_name)
    super(package_name,
          Helper.detect_version("pgroonga"),
          Helper.detect_release_time)
    @original_archive_base_name = "pgroonga-#{@version}"
    @original_archive_name = "#{@original_archive_base_name}.tar.gz"
  end

  private
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

  def apt_expand_variable(key)
    case key
    when "GROONGA_VERSION"
      latest_groonga_version
    else
      nil
    end
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
end

class VersionedPGroongaPackageTask < GenericPGroongaPackageTask
  def initialize(postgresql_version, postgresql_package_name_suffix="")
    @postgresql_version = postgresql_version
    @postgresql_package_version = postgresql_version.gsub(".", "")
    postgresql_package_name =
      "postgresql-#{@postgresql_version}#{postgresql_package_name_suffix}"
    super("#{postgresql_package_name}-pgroonga")
    rpm_postgresql_package_name =
      "postgresql#{@postgresql_version}#{postgresql_package_name_suffix}"
    @rpm_package = "#{rpm_postgresql_package_name}-pgroonga"
  end

  def define
    super
    define_yum_spec_in_task
  end

  private
  def apt_prepare_debian_control(control_in, target)
    substitute_content(control_in) do |key, matched|
      apt_expand_variable(key) || matched
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
    if enable_yum?
      cp(source_yum_spec_in_path, yum_spec_in_path)
    end
    super
  end
end
