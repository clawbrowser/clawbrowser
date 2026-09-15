#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fma_test_dir="$(mktemp -d)"
trap 'rm -rf -- "$fma_test_dir"' EXIT

# Compile the actual new header from the patch, not a copied implementation.
# This tiny numerical regression does not need a Chromium checkout or secrets.
git -C "$fma_test_dir" apply \
  --include=third_party/skia/src/opts/SkClawbrowserFma.h \
  "$repo_root/clawbrowser/patches/048-skia-fast-exact-baseline-fma.patch"

flags=(-std=c++20 -O2 -ffp-contract=off)
if [[ "$(uname -m)" == x86_64 ]]; then
  flags+=(-msse2 -mno-avx -mno-fma)
fi
"${CXX:-c++}" "${flags[@]}" \
  -I"$fma_test_dir/third_party/skia" \
  "$repo_root/clawbrowser/test/unit/skia_fma_rounding_test.cc" \
  -o "$fma_test_dir/fma-test"
"$fma_test_dir/fma-test"
