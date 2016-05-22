#!/usr/bin/env ruby
# compile itch for production environemnts

require_relative 'common'

module Capsule
  def Capsule.ci_compile
    say "Compiling #{app_name}"

    ✓ sh 'cmake'
    ✓ sh 'msbuild'
  end
end

Capsule.ci_compile

