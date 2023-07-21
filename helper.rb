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

  def detect_release_time
    release_time_env = ENV["RELEASE_TIME"] || ENV["NEW_RELEASE_DATE"]
    if release_time_env
      Time.parse(release_time_env).utc
    else
      Time.now.utc
    end
  end

  def detect_latest_groonga_version
    releases_uri = URI("https://api.github.com/repos/groonga/groonga/releases")
    releases_uri.open do |releases_output|
      releases = JSON.parse(releases_output.read)
      releases[0]["tag_name"].delete_prefix("v")
    end
  end
end
