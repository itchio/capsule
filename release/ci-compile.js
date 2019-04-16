#!/usr/bin/env node

// compile capsule for various platforms

const $ = require("./common");
const path = require("path");

let workspacePath = path.join(process.cwd());
let workspaceCargoPath = path.join(workspacePath, "Cargo.toml");

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
  $(
    await $.sh(
      `curl --fail --output "${name}" "https://static.rust-lang.org/rustup/dist/${platform}/${name}"`
    )
  );
  $(await $.sh(`chmod +x "${name}"`));
  $(await $.sh(`"./${name}" --no-modify-path -y`));
}

async function build_libcapsule({ test, testFlags, libName, runName, osarch, platform, strip }) {
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

  $(await $.sh(`rustup toolchain install "stable-${platform}"`));
  $(
    await $.sh(
      `rustup run "stable-${platform}" cargo build --target "${platform}" --release --manifest-path "${workspaceCargoPath}"`
    )
  );

  let dest = path.join("compile-artifacts", osarch);
  $(await $.sh(`mkdir -p "compile-artifacts/${osarch}"`));
  let libFile = `${workspacePath}/target/${platform}/release/${libName}`;
  let runFile = `${workspacePath}/target/${platform}/release/${runName}`;
  if (strip) {
    $(await $.sh(`strip "${libFile}"`));
    $(await $.sh(`strip "${runFile}"`));
  }
  $(await $.sh(`cp -rf "${libFile}" "${dest}"`));
  $(await $.sh(`cp -rf "${runFile}" "${dest}"`));

  if (test) {
    await $.cd("test", async () => {
      $(await $.sh(`gcc ${test}.c -o ${test} ${testFlags || ""}`));
      $(
        await $.sh(
          `CAPSULE_TEST=1 RUST_BACKTRACE=1 "${runFile}" ./${test} > out.txt`
        )
      );
      let expectedOutput = "caught dead beef";
      let actualOutput = (await $.readFile("out.txt")).trim();
      if (actualOutput == expectedOutput) {
        $.say(`Injection test passed!`);
      } else {
        throw new Error(
          `Injection test failed:\nexpected\n${expectedOutput}\ngot:${actualOutput}`
        );
      }
    });
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
      test: arch == "amd64" ? "win-test" : undefined,
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
    strip: true
  });
}

ci_compile(process.argv.slice(2)).catch(e => {
  console.error(e);
  process.exit(1);
});
