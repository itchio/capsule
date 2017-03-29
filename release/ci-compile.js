#!/usr/bin/env node

// compile capsule for various platforms

const $ = require('./common')

function ci_compile (args) {
  if (args.length < 1) {
    throw new Error(`ci-compile expects at least one argument, not ${args.length}. (got: ${args.join(', ')})`)
  }
  const [os] = args

  let arch = "";
  if (os === "linux") {
    if (args.length !== 2) {
      throw new Error(`ci-compile expects two arguments (os arch), not ${args.length}. (got: ${args.join(', ')})`)
    }
    arch = args[1];
  }

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
      ci_compile_linux(arch)
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
    $.cmd(['mkdir', 'compile-artifacts\\' + spec.osarch])
  }

  for (const spec of specs) {
    $.cmd(['rmdir', '/s', '/q', spec.dir])
    $.cmd(['mkdir', spec.dir])
    $.cd(spec.dir, function () {
      $($.cmd(['cmake', '-G', spec.generator, '..']))
      $($.cmd(['msbuild', 'INSTALL.vcxproj', '/p:Configuration=Release']))
    })

    $($.cmd(['xcopy', '/y', '/i', spec.dir + '\\dist\\*', 'compile-artifacts\\' + spec.osarch]))
  }
}

function ci_compile_darwin () {
  $.sh(`rm -rf compile-artifacts`)
  const osarch = 'darwin-amd64'

  $.sh('rm -rf build')
  $.sh('mkdir -p build')
  $.cd('build', function () {
    $($.sh('cmake .. -DOSX_UNIVERSAL=OFF'))
    $($.sh('make install'))
  })

  $.sh(`mkdir -p compile-artifacts/${osarch}/`)
  $($.sh(`cp -rf build/dist/* compile-artifacts/${osarch}/`))
}

function ci_compile_linux (arch) {
  $.sh(`rm -rf compile-artifacts`)

  const specs = [
    {
      dir: 'build32',
      os: 'linux',
      arch: '386'
    },
    {
      dir: 'build64',
      os: 'linux',
      arch: 'amd64'
    }
  ]

  let spec = null;
  for (const candidate of specs) {
    if (candidate.arch === arch) {
      spec = candidate;
    }
  }

  if (!spec) {
    throw new Error(`arch not handled: ${arch}`);
  }

  {
    const osarch = spec.os + '-' + spec.arch
    $.sh(`rm -rf ${spec.dir}`)
    $.sh(`mkdir -p ${spec.dir}`)
    $.cd(spec.dir, function () {
      $($.sh(`cmake -DCAPSULE_DEPS_PREFIX=/usr/capsule ..`))
      $($.sh(`make install`))
    })
    $.sh(`mkdir -p compile-artifacts/${osarch}`)

    $($.sh(`cp -rf ${spec.dir}/dist/* compile-artifacts/${osarch}/`))

    const libs = $.get_output('ldd build/capsulerun/capsulerun | grep -E "lib(av|sw)" | cut -d " " -f 1 | sed -E "s/^[[:space:]]*//g"').trim().split('\n')
    for (const lib of libs) {
      if (lib.length === 0) {
        continue
      }
      $($.sh(`cp -f ${spec.prefix}/lib/${lib} compile-artifacts/${osarch}/`))
    }
  }
}

ci_compile(process.argv.slice(2))
