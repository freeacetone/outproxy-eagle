#!/bin/sh
# Format all first-party C++ sources in-place with clang-format, using the
# style in .clang-format. Vendored code under third_party/ is left untouched.
#
# Usage:
#   ./format.sh           # reformat src/ in place
#   ./format.sh --check   # exit non-zero if any file is not formatted (CI)
set -e

cd "$(dirname "$0")"

SRC=$(find src -type f \( -name '*.cpp' -o -name '*.hpp' \))

if [ "$1" = "--check" ]; then
    clang-format --dry-run --Werror $SRC
    echo "format: all files are clean."
else
    clang-format -i $SRC
    echo "format: reformatted $(echo "$SRC" | wc -w) files."
fi
