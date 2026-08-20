#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

rm -rf web/dist/html
mkdir -p web/dist/html

node -e 'const fs = require("fs");
const s = fs.readFileSync("settings.txt", "utf8");
fs.writeFileSync("web/dist/html/settings.js", "const PUPLANG_SETTINGS = " + JSON.stringify(s) + ";\n");'

cp web/content/* web/dist/html
cp web/dist/puplang.js web/dist/html
cp web/dist/puplang.wasm web/dist/html