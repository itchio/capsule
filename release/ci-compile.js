#!/usr/bin/env node

// compile capsule for various platforms

const $ = require("./common");
const path = require("path");

let libPath = path.join(process.cwd(), "libcapsule");
let libCargoPath = path.join(libPath, "Cargo.toml");

async function ci_compile (args) {
  if (args.length < 1) {
    throw new Error(`ci-compile expects at least one argument, not ${args.length}. (got: ${args.join(", ")})`)
  }
  const [os] = args

  let arch = "";
  if (os === "linux") {
    if (args.length !== 2) {
      throw new Error(`ci-compile expects two arguments (os arch), not ${args.length}. (got: ${args.join(", ")})`)
    }
    arch = args[1];
  }

  $.say(`Compiling ${$.app_name()}`)
  switch (os) {
    case "windows": {
      await ci_compile_windows()
      break
    }
    case "darwin": {
      await ci_compile_darwin()
      break
    }
    case "linux": {
      await ci_compile_linux(arch)
      break
    }
    default: {
      throw new Error(`Unsupported os ${os}`)
    }
  }
}

function rustArch(arch) {
    switch (arch) {
      case "386":   return "i686"
      case "amd64": return "x86_64"
    }
    throw new Error(`Unsupported arch: ${arch}`);
}

async function install_rust({name, platform}) {
  if (!name) { throw new Error("missing name") }
  if (!platform) { throw new Error("missing platform") }

  process.env.CARGO_HOME = path.join(process.cwd(), ".cargo");
  process.env.RUSTUP_HOME = path.join(process.cwd(), ".multirust");
  process.env.PATH = `${process.cwd()}/.cargo/bin:${process.env.PATH}`;
  $(await $.sh(`curl --fail --output "${name}" "https://static.rust-lang.org/rustup/dist/${platform}/${name}"`));
  $(await $.sh(`chmod +x "${name}"`));
  $(await $.sh(`"./${name}" --no-modify-path -y`));
}

async function build_libcapsule({libName, osarch, platform}) {
  if (!libName) { throw new Error("missing libName") }
  if (!platform) { throw new Error("missing platform") }
  if (!osarch) { throw new Error("missing osarch") }

  $(await $.sh(`rustup toolchain install "stable-${platform}"`));
  $(await $.sh(`rustup run "stable-${platform}" cargo build --target "${platform}" --release --manifest-path "${libCargoPath}"`));

  let dest = path.join("compile-artifacts", osarch);
  $(await $.sh(`mkdir -p "compile-artifacts/${osarch}"`));
  $(await $.sh(`cp -rf "${libPath}/target/${platform}/release/${libName}" "compile-artifacts/${osarch}"`));
}

async function ci_compile_windows () {
  await install_rust({
    name: `rustup-init.exe`,
    platform: `x86_64-pc-windows-msvc`,
  });

  for (const arch of ["386", "amd64"]) {
    let platform = `${rustArch(arch)}-pc-windows-msvc`;
    await build_libcapsule({
      libName: `capsule.dll`,
      osarch: `windows-${arch}`,
      platform,
    });
  }
}

async function ci_compile_darwin() {
  let platform = `x86_64-apple-darwin`;
  await install_rust({
    name: `rustup-init`,
    platform,
  });
  await build_libcapsule({
    libName: `libcapsule.dylib`,
    osarch: `darwin-amd64`,
    platform,
  });
}

async function ci_compile_linux(arch) {
  let platform = `${rustArch(arch)}-unknown-linux-gnu`;
  await install_rust({
    name: `rustup-init`,
    platform,
  });
  await build_libcapsule({
    libName: `libcapsule.so`,
    osarch: `linux-${arch}`,
    platform,
  });
}

ci_compile(process.argv.slice(2)).catch((e) => {
  console.error(e);
  process.exit(1);
});

