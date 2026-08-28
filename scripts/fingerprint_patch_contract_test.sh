#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

webgl_patch="${repo_root}/clawbrowser/patches/008-webgl-override.patch"
cdp_patch="${repo_root}/clawbrowser/patches/033-devtools-no-getter-preview.patch"

if grep -q 'surface_policy.webgl == "override"' "${webgl_patch}"; then
  echo "WebGL backend values must not be hidden behind surface_policy.webgl." >&2
  exit 1
fi

grep -q '!fp->webgl.renderer.empty()' "${webgl_patch}"
grep -q '!fp->webgl.vendor.empty()' "${webgl_patch}"

if [[ ! -f "${cdp_patch}" ]]; then
  echo "Missing CDP getter-preview suppression patch." >&2
  exit 1
fi

grep -q 'v8/src/inspector/value-mirror.cc' "${cdp_patch}"
grep -q 'Clawbrowser does not invoke page getters while building CDP previews' "${cdp_patch}"
grep -q 'object->Get(context, v8Name)' "${cdp_patch}"
