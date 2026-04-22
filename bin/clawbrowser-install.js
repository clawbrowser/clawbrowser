#!/usr/bin/env node
const { spawnSync } = require("node:child_process");
const { join } = require("node:path");

const argv = process.argv.slice(2);
const command = argv[0];
if (command === "uninstall" || command === "remove") {
  console.error("Clawbrowser uninstall has been removed. Use the install script only.");
  process.exit(1);
}

const script = join(__dirname, "..", "scripts/install.sh");
const result = spawnSync(
  "bash",
  [script, ...argv],
  {
    stdio: "inherit",
  },
);

process.exit(result.status == null ? 1 : result.status);
