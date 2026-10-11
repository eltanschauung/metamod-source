#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "$0")/../.." && pwd)
test_build=$(mktemp -d)
trap 'rmdir "$test_build" 2>/dev/null || true' EXIT

for arch in 32 64; do
    "${CXX:-c++}" -m"$arch" -std=c++14 -O2 -Wall -Wextra -Werror \
        -isystem "$repo_dir/core/sourcehook" \
        "$repo_dir/tools/kogasa/map_parser_test.cpp" \
        -o "$test_build/map-parser-test-$arch"
    "$test_build/map-parser-test-$arch"
    # Only remove the exact executable generated in this private temporary dir.
    rm -- "$test_build/map-parser-test-$arch"
done
