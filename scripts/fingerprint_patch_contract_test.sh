#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

webgl_patch="${repo_root}/clawbrowser/patches/008-webgl-override.patch"
cdp_patch="${repo_root}/clawbrowser/patches/033-devtools-no-getter-preview.patch"
screen_patch="${repo_root}/clawbrowser/patches/006-screen-metrics.patch"
css_screen_patch="${repo_root}/clawbrowser/patches/036-css-media-screen.patch"
font_enum_patch="${repo_root}/clawbrowser/patches/011-fonts-filter.patch"
css_font_patch="${repo_root}/clawbrowser/patches/034-css-font-probing.patch"
font_fallback_patch="${repo_root}/clawbrowser/patches/035-font-fallback-probing.patch"

if grep -q 'surface_policy.webgl == "override"' "${webgl_patch}"; then
  echo "WebGL backend values must not be hidden behind surface_policy.webgl." >&2
  exit 1
fi

grep -q '!fp->webgl.renderer.empty()' "${webgl_patch}"
grep -q '!fp->webgl.vendor.empty()' "${webgl_patch}"
if grep -A8 'case GL_RENDERER:' "${webgl_patch}" | grep -q 'fp->webgl'; then
  echo "Masked GL_RENDERER must retain Chromium's standard value." >&2
  exit 1
fi
if grep -A8 'case GL_VENDOR:' "${webgl_patch}" | grep -q 'fp->webgl'; then
  echo "Masked GL_VENDOR must retain Chromium's standard value." >&2
  exit 1
fi

if [[ ! -f "${cdp_patch}" ]]; then
  echo "Missing CDP getter-preview suppression patch." >&2
  exit 1
fi

grep -q 'v8/src/inspector/value-mirror.cc' "${cdp_patch}"
grep -q 'Clawbrowser does not invoke page getters while building CDP previews' "${cdp_patch}"
grep -q 'object->Get(context, v8Name)' "${cdp_patch}"

# Release artifacts must include every patch needed to prevent host screen and
# font discovery. Checking these contracts here makes a missing patch fail the
# packaging build before an artifact is published.
grep -q 'fp->screen.width' "${screen_patch}"
grep -q 'fp->screen.pixel_ratio' "${screen_patch}"
grep -q 'fp->screen.width' "${css_screen_patch}"
grep -q 'ShouldFilterLocalFonts' "${font_enum_patch}"
grep -q 'IsLocalFontBlocked' "${css_font_patch}"
grep -q 'IsLocalFontBlocked' "${font_fallback_patch}"
