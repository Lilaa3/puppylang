#!/usr/bin/env bash
# Runs clang-format over all first-party C++ sources.
set -euo pipefail
cd "$(dirname "$0")"

command -v clang-format >/dev/null ||
    { echo "error: clang-format not installed" >&2; exit 1; }

while IFS= read -r -d '' file; do
    if ! cmp -s "${file}" <(clang-format "${file}"); then
        echo "formatting ${file}"
        clang-format -i "${file}"
    fi
done < <(
    find lib cli web \
        \( -path 'thirdparty/*' -o -path 'web/node_modules/*' \) -prune -o \
        \( -name '*.cpp' -o -name '*.hpp' \) -type f -print0
)
