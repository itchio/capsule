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
      ci_compile_capsulerun('windows', '386')
      ci_compile_capsulerun('windows', 'amd64')
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
      dll: 'capsule_x86.dll',
      generator: 'Visual Studio 14 2015'
    },
    {
      dir: 'build64',
      dll: 'capsule_x64.dll',
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
    $.cmd(['copy', '/y', spec.dir + '\\libcapsule\\Debug\\capsule.dll', 'compile-artifacts\\' + spec.dll])
  }
}

function ci_compile_capsulerun (os, arch) {
  process.env.CGO_ENABLED = '1'

  // set up go cross-compile
  $($.go('get github.com/mitchellh/gox'))

  let TRIPLET = ''
  if (os === 'windows') {
    if (arch === '386') {
      TRIPLET = 'i686-w64-mingw32-'
      process.env.PKG_CONFIG_PATH = 'C:\\msys64\\mingw32\\lib\\pkgconfig'
    } else {
      TRIPLET = 'x86_64-w64-mingw32-'
      process.env.PKG_CONFIG_PATH = 'C:\\msys64\\mingw64\\lib\\pkgconfig'
    }
  }

  if (os === 'linux') {
    // TODO: move on to something cleaner
    if (arch === '386') {
      process.env.PKG_CONFIG_PATH = '/ffmpeg/32/prefix/lib/pkgconfig'
    } else {
      process.env.PKG_CONFIG_PATH = '/ffmpeg/64/prefix/lib/pkgconfig'
    }
  }

  process.env.CC = `${TRIPLET}gcc`
  process.env.CXX = `${TRIPLET}g++`

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
  $.sh('rm -rf build')
  $.sh('mkdir -p build')
  $.cd('build', function () {
    $($.sh('cmake ..'))
    $($.sh('make'))
  })
  ci_compile_capsulerun('darwin', 'amd64')
}

function ci_compile_linux () {
  $.sh('rm -rf build')
  $.sh('mkdir -p build')
  $.cd('build', function () {
    $($.sh('cmake ..'))
    $($.sh('make'))
  })
  ci_compile_capsulerun('linux', '386')
  ci_compile_capsulerun('linux', 'amd64')
}

ci_compile(process.argv.slice(2))
