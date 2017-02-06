#!/usr/bin/env node

// compile capsule for various platforms

const $ = require('./common')

function ci_compile (args) {
  if (args.length !== 1) {
    throw new Error(`ci-compile expects one arguments, not ${args.length}. (got: ${args.join(', ')})`)
  }
  const [os] = args

  $.say(`Compiling ${$.app_name()}`)

  switch (os) {
    case 'windows': {
      ci_compile_windows()
      break
    }
    case 'darwin': {
      ci_compile_darwin()
      break
    }
    case 'linux': {
      ci_compile_linux()
      break
    }
    default: {
      throw new Error(`Unsupported os ${os}`)
    }
  }
}

function ci_compile_windows () {
  const specs = [
    {
      dir: 'build32',
      osarch: 'windows-386',
      generator: 'Visual Studio 14 2015'
    },
    {
      dir: 'build64',
      osarch: 'windows-amd64',
      generator: 'Visual Studio 14 2015 Win64'
    }
  ]

  $.cmd(['rmdir', '/s', '/q', 'compile-artifacts'])
  $.cmd(['mkdir', 'compile-artifacts'])
  for (const spec of specs) {
    $.cmd(['rmdir', '/s', '/q', spec.dir])
    $.cmd(['mkdir', spec.dir])
    $.cd(spec.dir, function () {
      $($.cmd(['cmake', '-G', spec.generator, '..']))
      $($.cmd(['msbuild', 'capsule.sln', '/p:Configuration=Release']))
    })
    $.cmd(['mkdir', 'compile-artifacts\\' + spec.osarch])

    $.cmd(['mkdir', 'compile-artifacts\\' + spec.osarch + '\\libcapsule'])
    $($.cmd(['copy', '/y', spec.dir + '\\libcapsule\\Release\\capsule.dll', 'compile-artifacts\\' + spec.osarch + '\\libcapsule\\capsule.dll']))

    $.cmd(['mkdir', 'compile-artifacts\\' + spec.osarch + '\\capsulerun'])
    $($.cmd(['copy', '/y', spec.dir + '\\capsulerun\\Release\\capsulerun.exe', 'compile-artifacts\\' + spec.osarch + '\\capsulerun\\capsulerun.exe']))
  }
}

function ci_compile_darwin () {
  $.sh(`rm -rf compile-artifacts`)
  const osarch = 'darwin-amd64'

  $.sh('rm -rf build')
  $.sh('mkdir -p build')
  $.cd('build', function () {
    $($.sh('cmake ..'))
    $($.sh('make'))
  })

  $.sh(`mkdir -p compile-artifacts/libcapsule/${osarch}/`)
  $($.sh(`cp -rf build/libcapsule/libcapsule.dylib compile-artifacts/libcapsule/${osarch}/`))

  $.sh(`mkdir -p compile-artifacts/capsulerun/${osarch}/`)
  $($.sh(`cp -rf build/capsulerun/capsulerun compile-artifacts/capsulerun/${osarch}/`))
}

function ci_compile_linux () {
  $.sh(`rm -rf compile-artifacts`)

  const specs = [
    {
      dir: 'build32',
      cmake_extra: '-DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-Linux32.cmake',
      os: 'linux',
      arch: '386',
      prefix: '/ffmpeg/32/prefix'
    },
    {
      dir: 'build64',
      cmake_extra: '',
      os: 'linux',
      arch: 'amd64',
      prefix: '/ffmpeg/64/prefix'
    }
  ]

  for (const spec of specs) {
    const osarch = spec.os + '-' + spec.arch
    $.sh(`rm -rf ${spec.dir}`)
    $.sh(`mkdir -p ${spec.dir}`)
    $.cd(spec.dir, function () {
      $($.sh(`cmake .. ${spec.cmake_extra} -DCAPSULE_FFMPEG_PREFIX=${spec.prefix}`))
      $($.sh(`make`))
    })
    $.sh(`mkdir -p compile-artifacts/libcapsule/${osarch}`)
    $($.sh(`cp -rf ${spec.dir}/libcapsule/libcapsule.so compile-artifacts/libcapsule/${osarch}/`))

    $.sh(`mkdir -p compile-artifacts/capsulerun/${osarch}`)
    $($.sh(`cp -rf ${spec.dir}/capsulerun/capsulerun compile-artifacts/capsulerun/${osarch}/`))
    const libs = $.get_output('ldd build/capsulerun/capsulerun | grep -E "lib(av|sw)" | cut -d " " -f 1 | sed -E "s/^[[:space:]]*//g"').trim().split('\n')
    for (const lib of libs) {
      if (lib.length === 0) {
        continue
      }
      $($.sh(`cp -f ${spec.prefix}/lib/${lib} compile-artifacts/capsulerun/${osarch}/`))
    }
  }
}

ci_compile(process.argv.slice(2))
