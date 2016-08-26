#!/usr/bin/env node

// compile itch for production environemnts

const $ = require('./common')

function ci_compile (args) {
  if (args.length !== 1) {
    throw new Error(`ci-compile expects one arguments, not ${args.length}. (got: ${args.join(', ')})`)
  }
  const [os] = args

  $.say(`Compiling ${$.app_name()}`)

  switch (os) {
    case 'windows-msbuild': {
      ci_compile_windows_msbuild()
      break
    }
    case 'windows-mingw': {
      ci_compile_windows_mingw()
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

function ci_compile_windows_msbuild () {
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
      $($.cmd(['msbuild', 'ALL_BUILD.vcxproj']))
    })
    $.cmd(['mkdir', 'compile-artfiacts\\' + spec.osarch])
    $.cmd(['copy', '/y', spec.dir + '\\libcapsule\\Debug\\capsule.dll', 'compile-artifacts\\' + spec.osarch + '\\capsule.dll'])
  }
}

function ci_compile_capsulerun (os, arch, opts) {
  if (!opts) {
    opts = {}
  }
  process.env.CGO_ENABLED = '1'

  // set up go cross-compile
  $($.go('get github.com/mitchellh/gox'))

  if (opts.prefix) {
    process.env.PKG_CONFIG_PATH = path.join(opts.prefix, 'lib', 'pkgconfig')
  }

  if (opts.cc) {
    process.env.CC = opts.cc
  }
  if (opts.cxx) {
    process.env.CXX = opts.cxx
  }

  let TARGET = 'capsulerun/capsulerun'

  if (os === 'windows') {
    TARGET += '.exe'
  }

  const pkg = 'github.com/itchio/capsule'
  $.sh(`mkdir -p ${pkg}`)

  // rsync will complain about vanishing files sometimes, who knows where they come from
  $($.sh(`rm -rf github.com`))
  $($.sh(`rsync -az . ${pkg} || echo "rsync complained (code $?)"`))

  // grab deps
  process.env.GOOS = os
  process.env.GOARCH = arch
  $($.go(`get -v -d -t ${pkg}/capsulerun`))
  delete process.env.GOOS
  delete process.env.GOARCH

  // compile
  // todo: LDFLAGS?
  $.say(`PATH before gox: ${process.env.PATH}`)
  $($.sh(`gox -osarch "${os}/${arch}" -cgo -output="capsulerun/capsulerun" ${pkg}/capsulerun`))

  // sign (win)
  if (os === 'windows') {
    const WIN_SIGN_KEY = 'Open Source Developer, Amos Wenger'
    const WIN_SIGN_URL = 'http://timestamp.verisign.com/scripts/timstamp.dll'

    $($.sh(`signtool.exe sign //v //s MY //n "${WIN_SIGN_KEY}" //t "${WIN_SIGN_URL}" ${TARGET}`))
  }

  $.sh(`rm -rf compile-artifacts`)
  $.sh(`mkdir compile-artifacts`)
  $.sh(`cp ${TARGET} compile-artifacts`)
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

  ci_compile_capsulerun('darwin', 'amd64')
  $.sh(`mkdir -p compile-artifacts/libcapsule/${osarch}/`)
  $.sh(`cp -rf build/libcapsule/libcapsule.dylib compile-artifacts/libcapsule/${osarch}/`)
}

function ci_compile_linux () {
  $.sh(`rm -rf compile-artifacts`)

  const specs = [
    {
      dir: 'build32',
      cmake_extra: '-DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-Linux32.cmake',
      os: 'linux',
      arch: '386',
      prefix: '/ffmpeg/32/prefix',
    },
    {
      dir: 'build64',
      cmake_extra: '',
      os: 'linux',
      arch: 'amd64',
      prefix: '/ffmpeg/64/prefix',
    }
  ]

  for (const spec of specs) {
    const osarch = spec.os + '-' + spec.arch
    $.sh(`rm -rf ${spec.dir}`)
    $.sh(`mkdir -p ${spec.dir}`)
    $.cd(spec.dir, function () {
      $($.sh(`cmake .. ${spec.cmake_extra}`))
      $($.sh(`make`))
    })
    $.sh(`mkdir -p compile-artifacts/libcapsule/${osarch}`)
    $.sh(`cp -rf ${spec.dir}/libcapsule/libcapsule.so compile-artifacts/libcapsule/${osarch}/`)

    ci_compile_capsulerun(spec.os, spec.arch, {
      prefix: spec.prefix
    })
    $.sh(`mkdir -p compile-artifacts/capsulerun/${osarch}`)
    $.sh(`cp -rf capsulerun/capsulerun compile-artifacts/capsulerun/${osarch}/`)
    const libs = $.get_output('ldd capsulerun/capsulerun | grep -E "lib(av|sw)" | cut -d " " -f 1 | sed -E "s/^[[:space:]]*//g"').trim().split('\n')
    for (const lib of libs) {
      $.sh(`cp -f ${spec.prefix}/lib/${lib} compile-artifacts/capsulerun/${osarch}/`)
    }
  }
}

function ci_compile_mingw () {
  $.sh(`rm -rf compile-artifacts`)
  
  const specs = [
    {
      os: 'windows',
      arch: '386',
      prefix: 'C:\\msys64\\mingw32',
      cc: 'i686-w64-mingw32-gcc',
      cxx: 'i686-w64-mingw32-g++'
    },
    {
      os: 'windows',
      arch: 'amd64',
      prefix: 'C:\\msys64\\mingw64',
      cc: 'x86_64-w64-mingw32-gcc',
      cxx: 'x86_64-w64-mingw32-g++'
    },
  ]

  for (const spec of specs) {
    const osarch = spec.os + '-' + spec.arch
    ci_compile_capsulerun(spec.os, spec.arch, {
      prefix: spec.prefix,
      cc: spec.cc,
      cxx: spec.cxx
    })

    $.sh(`mkdir -p compile-artifacts/capsulerun/${osarch}`)
    $.sh(`cp -rf capsulerun/capsulerun.exe compile-artifacts/capsulerun/${osarch}/`)
  }
}

ci_compile(process.argv.slice(2))
