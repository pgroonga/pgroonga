module Helper
  module_function
  def env_value(name)
    value = ENV[name]
    raise "Specify #{name} environment variable" if value.nil?
    value
  end

  def control_file(package)
    File.join(__dir__, "#{package}.control")
  end

  def detect_version(package)
    env_version = ENV["VERSION"]
    return env_version if env_version

    control_content = File.read(control_file(package))
    if /^default_version\s*=\s*'(.+)'$/ =~ control_content
      $1
    else
      nil
    end
  end

  def detect_latest_groonga_version
    URI("https://packages.groonga.org/source/groonga/").open do |groonga_sources|
      versions = groonga_sources.read.scan(/<a href="groonga-([\d.]+).zip">/)
      versions.flatten.sort.last
    end
  end
end
