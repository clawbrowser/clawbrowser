#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
half_test_dir="$(mktemp -d)"
trap 'rm -rf -- "$half_test_dir"' EXIT
git -C "$half_test_dir" apply --include=third_party/skia/src/opts/SkClawbrowserHalf.h \
  "$repo_root/clawbrowser/patches/050-skia-exact-baseline-half.patch"
"${CXX:-c++}" -std=c++20 -O2 -I"$half_test_dir/third_party/skia" \
  "$repo_root/clawbrowser/test/unit/skia_half_test.cc" -o "$half_test_dir/half-test"
"$half_test_dir/half-test"
