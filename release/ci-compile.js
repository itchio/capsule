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
      ci_compile_capsulerun()
      break
    }
    case 'darwin': {
      ci_compile_darwin
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
      generator: 'Visual Studio 14 2015'
    },
    {
      dir: 'build64',
      generator: 'Visual Studio 14 2015 Win64'
    }
  ]

  for (const spec of specs) {
    $.cmd(['rmdir', '/s', '/q', spec.dir])
    $.cmd(['mkdir', spec.dir])
    $.cd(spec.dir, function () {
      $($.cmd(['cmake', '-G', spec.generator, '..']))
      $($.cmd(['msbuild', 'ALL_BUILD.vcxproj']))
    })
  }
}

function ci_compile_capsulerun () {
  $.cd('capsulerun', function () {
    $($.go('build capsulerun.go'))
  })
}

function ci_compile_darwin () {
  $.rm_rf('build')
  $.mkdir_p('build')
  $.cd('build', function () {
    $($.sh('cmake ..'))
    $($.sh('make'))
  })
  ci_compile_capsulerun()
}

ci_compile(process.argv.slice(2))
