#!/usr/bin/env node

// compile capsule for various platforms

const $ = require("./common");
const path = require("path");

let workspacePath = path.join(process.cwd());

async function ci_compile(args) {
  if (args.length < 1) {
    throw new Error(
      `ci-compile expects at least one argument, not ${
        args.length
      }. (got: ${args.join(", ")})`
    );
  }
  const [os] = args;

  let arch = "";
  if (os === "linux") {
    if (args.length !== 2) {
      throw new Error(
        `ci-compile expects two arguments (os arch), not ${
          args.length
        }. (got: ${args.join(", ")})`
      );
    }
    arch = args[1];
  }

  $.say(`Compiling ${$.app_name()}`);
  switch (os) {
    case "windows": {
      await ci_compile_windows();
      break;
    }
    case "darwin": {
      await ci_compile_darwin();
      break;
    }
    case "linux": {
      await ci_compile_linux(arch);
      break;
    }
    default: {
      throw new Error(`Unsupported os ${os}`);
    }
  }
}

function rustArch(arch) {
  switch (arch) {
    case "386":
      return "i686";
    case "amd64":
      return "x86_64";
  }
  throw new Error(`Unsupported arch: ${arch}`);
}

async function download_file_if_needed(url, dest) {
  $.say(`Downloading (${url}) to (${dest})`);
  // see https://superuser.com/a/908523
  $(
    await $.sh(`curl --fail --output "${dest}" --time-cond "${dest}" "${url}"`)
  );
}

async function install_rust({ name, platform }) {
  if (!name) {
    throw new Error("missing name");
  }
  if (!platform) {
    throw new Error("missing platform");
  }

  process.env.CARGO_HOME = path.join(process.cwd(), ".cargo");
  process.env.RUSTUP_HOME = path.join(process.cwd(), ".multirust");
  let cargoBinPath = path.join(process.cwd(), ".cargo", "bin");
  process.env.PATH = `${cargoBinPath}${path.delimiter}${process.env.PATH}`;
  $.say(`Modified $PATH: ${process.env.PATH}`);
  let rustupURL = `https://static.rust-lang.org/rustup/dist/${platform}/${name}`;
  await download_file_if_needed(rustupURL, name);
  $(await $.sh(`chmod +x "${name}"`));
  $(await $.sh(`"./${name}" --no-modify-path -y`));
}

async function build_libcapsule(opts) {
  let { tests, libName, runName, osarch, platform, strip } = opts;
  if (!libName) {
    throw new Error("missing libName");
  }
  if (!runName) {
    throw new Error("missing runName");
  }
  if (!platform) {
    throw new Error("missing platform");
  }
  if (!osarch) {
    throw new Error("missing osarch");
  }

  let chain = `stable-${platform}`;
  $(await $.sh(`rustup toolchain install ${chain}`));
  let cargoCmd = `cargo build --target "${platform}" --release`;
  $(await $.sh(`rustup run ${chain} ${cargoCmd}`));

  let dest = path.join("compile-artifacts", osarch);
  $(await $.sh(`mkdir -p "compile-artifacts/${osarch}"`));
  let libFile = `${workspacePath}/target/${platform}/release/${libName}`;
  let runFile = `${workspacePath}/target/${platform}/release/${runName}`;

  if (tests && tests.length > 0) {
    for (const test of tests) {
      if (typeof test.name !== "string") {
        throw new Error(`missing test name`);
      }
      if (typeof test.platform !== "string") {
        throw new Error(`missing test platform`);
      }
      $.say(`Running test ${test.name}`);

      let testDir = path.join("test", test.name);
      await $.cd(testDir, async () => {
        let chain = `stable-${test.platform}`;
        let cargoCmd = `cargo build --target ${test.platform}`;
        $(await $.sh(`rustup run ${chain} ${cargoCmd}`));
        let testFile = path.join("./target", test.platform, "debug", test.name);
        let runCmd = `CAPSULE_TEST=1 RUST_BACKTRACE=1 "${runFile}" "${testFile}"`;
        let actualOutput = (await $.getOutput(runCmd)).trim();
        let expectedOutput = "caught dead beef";
        if (actualOutput == expectedOutput) {
          $.say(`Injection test passed!`);
        } else {
          throw new Error(
            `Injection test failed:\nexpected\n${expectedOutput}\ngot:${actualOutput}`
          );
        }
      });
    }
  } else {
    $.say(`No tests!`);
  }

  $(await $.sh(`cp -f "${libFile}" "${dest}"`));
  $(await $.sh(`cp -f "${runFile}" "${dest}"`));
  if (strip) {
    $(await $.sh(`strip "${dest}/${libName}"`));
    $(await $.sh(`strip "${dest}/${runName}"`));
  }
}

async function ci_compile_windows() {
  await install_rust({
    name: `rustup-init.exe`,
    platform: `x86_64-pc-windows-msvc`
  });

  for (const arch of ["386", "amd64"]) {
    let platform = `${rustArch(arch)}-pc-windows-msvc`;
    await build_libcapsule({
      libName: `capsule.dll`,
      runName: `capsulerun.exe`,
      osarch: `windows-${arch}`,
      platform,
      tests: [
        {
          name: "windows-opengl-loadlibrary",
          platform
        }
      ]
    });
  }
}

async function ci_compile_darwin() {
  let platform = `x86_64-apple-darwin`;
  await install_rust({
    name: `rustup-init`,
    platform
  });
  await build_libcapsule({
    libName: `libcapsule.dylib`,
    runName: `capsulerun`,
    osarch: `darwin-amd64`,
    platform
  });
}

async function ci_compile_linux(arch) {
  let platform = `${rustArch(arch)}-unknown-linux-gnu`;
  await install_rust({
    name: `rustup-init`,
    platform
  });
  await build_libcapsule({
    libName: `libcapsule.so`,
    runName: `capsulerun`,
    osarch: `linux-${arch}`,
    platform,
    strip: true,
    tests: [
      {
        name: "linux-opengl-dlopen",
        platform
      }
    ]
  });
}

ci_compile(process.argv.slice(2)).catch(e => {
  $.say(e.stack || `${e}`);
  process.exit(1);
});
