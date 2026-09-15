#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_over_test_dir="$(mktemp -d)"
trap 'rm -rf -- "$source_over_test_dir"' EXIT
git -C "$source_over_test_dir" apply \
  --include=third_party/skia/src/opts/SkClawbrowserSrcOver.h \
  "$repo_root/clawbrowser/patches/049-skia-exact-bitmap-source-over.patch"
flags=(-std=c++20 -O2)
if [[ "$(uname -m)" == x86_64 ]]; then
  flags+=(-msse2 -mno-avx -DCLAWBROWSER_TEST_REQUIRE_SSE2=1)
  if [[ "${CLAWBROWSER_TEST_AVX2:-0}" == 1 ]]; then flags+=(-mavx2); fi
fi
"${CXX:-c++}" "${flags[@]}" -I"$source_over_test_dir/third_party/skia" \
  "$repo_root/clawbrowser/test/unit/skia_source_over_test.cc" -o "$source_over_test_dir/source-over-test"
"$source_over_test_dir/source-over-test"
