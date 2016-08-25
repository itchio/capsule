#!/usr/bin/env ruby
# compile itch for production environemnts

require_relative 'common'

module Capsule
  def Capsule.ci_compile (args)
    raise "ci-compile expects one argument" unless args.length == 1
    const [os] = args

    say "Compiling #{app_name}"

    case os
    when "windows-msbuild"
      ci_compile_windows_msbuild
    when "windows-mingw"
      ci_compile_capsulerun
    when "darwin"
      ci_compile_darwin
    else
      raise "Unsupported os #{os}"
    end
  end

  def Capsule.ci_compile_windows_msbuild
    FileUtils.rm_rf 'build32'
    FileUtils.mkdir_p 'build32'
    cd 'build32' do
      ✓ sh 'cmake -G "Visual Studio 14 2015" ..'
      ✓ sh 'msbuild ALL_BUILD.vcxproj'
    end

    FileUtils.rm_rf 'build64'
    FileUtils.mkdir_p 'build64'
    cd 'build64' do
      ✓ sh 'cmake -G "Visual Studio 14 2015 Win64" ..'
      ✓ sh 'msbuild ALL_BUILD.vcxproj'
    end
  end

  def Capsule.ci_compile_capsulerun
    cd 'capsulerun' do
      ✓ go 'build capsulerun.go'
    end
  end

  def Capsule.ci_compile_darwin
    FileUtils.rm_rf 'build'
    FileUtils.mkdir_p 'build'
    cd 'build' do
      ✓ sh 'cmake ..'
      ✓ sh 'make'
    end
    ci_compile_capsulerun
  end
end

Capsule.ci_compile ARGV
