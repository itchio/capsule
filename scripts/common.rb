# base functions useful throughout CI scripts

# avert your eyes for a minute...
ENV['PATH'] += ":#{Gem.user_dir}/bin"
system 'bundle install' or raise 'Bundle install failed!'
require 'rubygems'
require 'bundler/setup'
ENV['LANG']='C'
ENV['LANGUAGE']='C'
ENV['LC_ALL']='C'
# all good! you may resume reading the code

require 'colored' # colors make me happy
require 'json' # parse a bunch of templates
require 'time' # rfc2822
require 'filesize' # bytes are for computers
require 'digest' # md5 sums for debian

module Capsule
  HOMEPAGE = 'https://github.com/itchio/capsule'
  MAINTAINER = 'Amos Wenger <amos@itch.io>'
  DESCRIPTION = 'Capture OpenGL/Direct3D/Vulkan footage'

  BUILD_TIME = Time.now.utc

  RETRY_COUNT = 5
  HOME = ENV['HOME']

  # Supported operating systems
  OSES = {
    'windows' => {

    },
    'darwin' => {

    },
    'linux' => {

    },
  }

  # Supported architectures
  ARCHES = {
    '386' => {
    },
    'amd64' => {
    }
  }

  # local golang executables
  GOPATH = "#{HOME}/go"
  ENV['GOPATH'] = GOPATH
  FileUtils.mkdir_p GOPATH
  ENV['PATH'] += ":#{GOPATH}/bin"

  # local npm executables
  ENV['PATH'] += ":#{Dir.pwd}/node_modules/.bin"

  VERSION_SPECS = {
    '7za' => '7za | head -2',
    'node' => 'node --version',
    'npm' => 'npm --version',
    'gsutil' => 'gsutil --version',
    'go' => 'go version',
    'gothub' => 'gothub --version',
    'fakeroot' => 'fakeroot -v',
    'ar' => 'ar --version | head -1'
  }

  def Capsule.putln (s)
    puts s
    $stdout.flush
    true
  end

  def Capsule.show_versions (names)
    names.each do |name|
      v = ♫(VERSION_SPECS[name]).strip
      putln %Q{★ #{name} #{v}}.yellow
    end
  end

  def Capsule.say (cmd)
    putln %Q{♦ #{cmd}}.yellow
  end

  def Capsule.sh (cmd)
    putln %Q{· #{cmd}}.blue
    system cmd
  end

  def Capsule.qsh (cmd)
    putln %Q{· <redacted>}.blue
    system cmd
  end

  # run npm command (silently)
  def Capsule.npm (args)
    sh %Q{npm --no-progress --quiet #{args}}
  end

  # run gem command
  def Capsule.gem (args)
    sh %Q{gem #{args}}
  end

  # run grunt command
  def Capsule.grunt (args)
    sh %Q{grunt #{args}}
  end

  # run go command
  def Capsule.go (args)
    sh %Q{go #{args}}
  end

  # copy files to google cloud storage using gsutil
  def Capsule.gcp (args)
    sh %Q{gsutil -m cp -r -a public-read #{args}}
  end

  # manage github assets
  def Capsule.gothub (args)
    ENV['GITHUB_USER']='itchio'
    ENV['GITHUB_REPO']=app_name
    sh %Q{gothub #{args}}
  end

  # install a gem dep if missing
  def Capsule.gem_dep (cmd, pkg)
    if system %Q{which #{cmd} > /dev/null}
      putln "★ got #{cmd}".yellow
      true
    else
      putln "☁ installing #{cmd}".yellow
      gem "install #{pkg}"
    end
  end

  # install a node.js dep if missing
  def Capsule.npm_dep (cmd, pkg)
    if system %Q{which #{cmd} > /dev/null}
      putln "★ got #{cmd}".yellow
      true
    else
      putln "☁ installing #{cmd}".yellow
      npm "install -g #{pkg}"
    end
  end

  # install a golang project if missing
  def Capsule.go_dep (cmd, pkg)
    if system %Q{which #{cmd} > /dev/null}
      putln "★ got #{cmd}".yellow
      true
    else
      putln "☁ installing #{cmd}".yellow
      go "get #{pkg}"
    end
  end

  # enforce success of a command
  def Capsule.✓ (val)
    raise "Non-zero exit code, bailing out" unless val
  end

  # retry command a few times before giving up
  def Capsule.↻
    tries = 0
    while tries < RETRY_COUNT
      if tries > 0
        say "Command failed, waiting 30s then trying #{RETRY_COUNT - tries} more time(s)."
        sleep 30
      end
      return if yield # cmd returned truthy value, was successful
      tries += 1
    end
    raise "Tried #{RETRY_COUNT} times, bailing out"
  end

  # enforce success of a command & return output
  def Capsule.♫ (cmd)
    out = `#{cmd}`
    code = $?.to_i
    raise "Non-zero exit code, bailing out" unless code == 0
    out
  end

  def Capsule.prompt (msg)
    print "#{msg}: "
    $stdout.flush
    gets.strip
  end

  def Capsule.yesno (msg)
    print "#{msg} (y/n) "
    $stdout.flush

    unless "y" == gets.strip
      say "Bailing out..."
      exit 0
    end
  end

  def Capsule.cd (dir)
    putln "☞ entering #{dir}"
    Dir.chdir(dir) do
      yield
    end
    putln "☜ leaving #{dir}"
  end

  # environment variables etc.

  def Capsule.build_ref_name
    ENV['CI_BUILD_REF_NAME'] or raise "No build ref!"
  end

  def Capsule.build_tag
    ENV['CI_BUILD_TAG'] or raise "No build tag!"
  end

  def Capsule.build_version
    build_tag.gsub(/^v/, '').gsub(/-.+$/, '')
  end

  def Capsule.app_name
    return "capsule"
  end

  def Capsule.build_time
    return BUILD_TIME
  end
end

