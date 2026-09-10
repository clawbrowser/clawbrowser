#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

webgl_patch="${repo_root}/clawbrowser/patches/008-webgl-override.patch"
cdp_patch="${repo_root}/clawbrowser/patches/033-devtools-no-getter-preview.patch"
renderer_loader_patch="${repo_root}/clawbrowser/patches/002-renderer-main-loader.patch"
gpu_loader_patch="${repo_root}/clawbrowser/patches/003-gpu-main-loader.patch"
child_switch_patch="${repo_root}/clawbrowser/patches/004-child-process-flag-propagation.patch"
startup_source="${repo_root}/clawbrowser/startup.cc"
loader_source="${repo_root}/clawbrowser/fingerprint_loader.cc"
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

# A managed browser must never degrade into native fingerprinting. Browser
# startup exits on acquisition/persistence/load failures, and renderer/GPU
# children exit when their required payload cannot be loaded.
if grep -q 'FallbackToVanillaBrowser' "${startup_source}"; then
  echo "Managed fingerprint failures must not fall back to vanilla." >&2
  exit 1
fi
if grep -A20 'void ConfigureAuthStartup' "${startup_source}" |
    grep -q 'GetArgs'; then
  echo "Auth startup must not retain arbitrary target arguments." >&2
  exit 1
fi
grep -q 'fingerprint_api_unavailable' "${startup_source}"
grep -q 'fingerprint_save_failed' "${startup_source}"
grep -q 'fingerprint_load_failed' "${startup_source}"
grep -q 'fingerprint_child_payload_failed' "${startup_source}"
grep -q 'AppendSwitch(kRequireFingerprintSwitch)' "${startup_source}"
grep -q 'HasSwitch(kRequireFingerprintSwitch)' "${loader_source}"
grep -q 'managed child process is missing fingerprint payload and path' \
  "${loader_source}"

if ! grep -A2 '\[clawbrowser\] renderer:' "${renderer_loader_patch}" |
    grep -q '^+    return 1;$'; then
  echo "Renderer fingerprint load failures must terminate the child." >&2
  exit 1
fi
if ! grep -A2 '\[clawbrowser\] gpu:' "${gpu_loader_patch}" |
    grep -q '^+    return 1;$'; then
  echo "GPU fingerprint load failures must terminate the child." >&2
  exit 1
fi
grep -q 'clawbrowser-require-fingerprint' "${child_switch_patch}"

# Release artifacts must include every patch needed to prevent host screen and
# font discovery. Checking these contracts here makes a missing patch fail the
# packaging build before an artifact is published.
grep -q 'fp->screen.width' "${screen_patch}"
grep -q 'fp->screen.pixel_ratio' "${screen_patch}"
grep -q 'fp->screen.width' "${css_screen_patch}"
grep -q 'ShouldFilterLocalFonts' "${font_enum_patch}"
grep -q 'IsLocalFontBlocked' "${css_font_patch}"
grep -q 'IsLocalFontBlocked' "${font_fallback_patch}"
