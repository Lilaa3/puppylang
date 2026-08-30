#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

rm -rf web/dist/html
mkdir -p web/dist/html

cp -r web/content/* web/dist/html
mkdir -p web/dist/html/licenses
cp -r web/licenses/* web/dist/html/licenses
cp LICENSE web/dist/html/licenses/puplang-LICENSE.txt
cp thirdparty/argparse/LICENSE web/dist/html/licenses/argparse-LICENSE.txt
cp thirdparty/utf8/LICENSE web/dist/html/licenses/utfcpp-LICENSE.txt
cp -r images web/dist/html
cp web/dist/puplang.js web/dist/html
cp web/dist/puplang.wasm web/dist/html
node web/md2html.js