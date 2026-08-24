#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

mkdir -p build-web web/dist

emcmake cmake -S . -B build-web \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_CLI=OFF \
  -DBUILD_WASM=ON \
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$(pwd)/web/dist"

cmake --build build-web --target puplang-wasm -j

bash web/copy_over.sh
