#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

mkdir -p web/dist

em++ -std=c++26 -O3 \
  -Icpp-impl \
  web/puplang-wasm.cpp \
  -o web/dist/puplang.js \
  -sMODULARIZE=1 \
  -sEXPORT_NAME=createPuplangModule \
  -sEXPORTED_FUNCTIONS=_puplang_encode,_puplang_decode,_puplang_last_error,_malloc,_free \
  -sEXPORTED_RUNTIME_METHODS=ccall \
  -sALLOW_MEMORY_GROWTH=1 \
  -sFILESYSTEM=0 \
  -sENVIRONMENT=web,node \
  -sSTRICT=1

bash web/copy_over.sh