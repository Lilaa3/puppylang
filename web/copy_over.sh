#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

rm -rf web/dist/html
mkdir -p web/dist/html

cp web/content/* web/dist/html
cp -r images web/dist/html
cp web/dist/puplang.js web/dist/html
cp web/dist/puplang.wasm web/dist/html
node web/md2html.js