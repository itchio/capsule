#!/usr/bin/env node

const fs = require("fs");
const path = require("path");
const cp = require("child_process");

log = (msg) => { console.log(msg); }

function main () {
  const ourPath = process.argv.find((f) => /\.js$/.test(f));
  const dest = path.resolve(ourPath, "..", "..", "build", "dist");
  log(`Working in ${dest}`);

  const files = fs.readdirSync(dest);
  const machOFiles = files.filter((f) => {
    const fullPath = path.join(dest, f);
    const fileOutput = String(cp.execSync(`file "${fullPath}"`));
    return /Mach-O/.test(fileOutput);
  });

  let numLibsFixed = 0;
  for (const f of machOFiles) {
    // I could swear I've written this script before...
    const fullPath = path.join(dest, f);
    const otoolOutput = String(cp.execSync(`otool -L "${fullPath}"`));
    const lines = otoolOutput.split("\n");
    const relativeDeps = [];
    for (const line of lines) {
      const matches = /^\s+([^(]+)/.exec(line);
      if (!(matches && matches.length >= 1)) { continue; }
      const lib = matches[1].trim();
      if (/^\//.test(lib)) {
	// absolute path, skip
	continue;
      }
      if (/^@/.test(lib)) {
	// already relative to @loader_path or something, skip
	continue;
      }
      if (lib === path.basename(fullPath)) {
	// that's not a dependency, that's just us!
        continue;
      }

      const cmd = `install_name_tool -change ${lib} @loader_path/${lib} ${fullPath}`;
      log(`Fixing reference to ${lib} in ${path.basename(fullPath)}`);
      numLibsFixed++;
      cp.execSync(cmd);
    }
  }

  log(`${numLibsFixed} library paths fixed.`);
  log(`${path.join(dest, "capsulerun")} should now run from anywhere`);
}

main();
