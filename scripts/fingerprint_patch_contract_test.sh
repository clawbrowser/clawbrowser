#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

webgl_patch="${repo_root}/clawbrowser/patches/008-webgl-override.patch"
canvas_patch="${repo_root}/clawbrowser/patches/007-canvas-noise.patch"
startup_source="${repo_root}/clawbrowser/startup.cc"
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
startup_source="${repo_root}/clawbrowser/startup.cc"
verify_script="${repo_root}/clawbrowser/verify/resources/verify.js"

if grep -q 'surface_policy.webgl == "override"' "${webgl_patch}"; then
  echo "WebGL backend values must not be hidden behind surface_policy.webgl." >&2
  exit 1
fi

grep -q '!fp->webgl.renderer.empty()' "${webgl_patch}"
grep -q '!fp->webgl.vendor.empty()' "${webgl_patch}"
if grep -q 'ReadPixelsHelper\|PixelNoise' "${webgl_patch}"; then
  echo "WebGL readPixels must remain native to the coherent SwiftShader backend." >&2
  exit 1
fi
if grep -A8 'case GL_RENDERER:' "${webgl_patch}" | grep -q 'fp->webgl'; then
  echo "Masked GL_RENDERER must retain Chromium's standard value." >&2
  exit 1
fi

# Fingerprint profiles must select one real software adapter instead of only
# replacing its renderer string. The backend selection also has to win over a
# stale runtime_gpu value replayed from a cached request.
grep -q 'AppendSwitchASCII("use-gl", "angle")' "${startup_source}"
grep -q 'AppendSwitchASCII("use-angle", "swiftshader")' "${startup_source}"
grep -q 'request->runtime_gpu = RuntimeGPUHint(command_line)' "${startup_source}"
grep -q 'policy.canvas != "override"' "${startup_source}"
grep -q '!IsSwiftShaderWebGLBackend' "${startup_source}"

# Canvas noise may perturb color channels, but never the alpha byte.
grep -q 'channel < 3' "${canvas_patch}"
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
grep -A35 'void ApplyFingerprintWebGLIsolation' "${startup_source}" |
  grep -q 'AppendSwitch(kDisableWebGLSpoofingSwitch)'
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
grep -A4 'int Screen::availLeft() const' "${screen_patch}" | grep -q 'FingerprintAccessor::Get()'
grep -A4 'int Screen::availTop() const' "${screen_patch}" | grep -q 'FingerprintAccessor::Get()'
grep -A4 'int LocalDOMWindow::screenX() const' "${screen_patch}" | grep -q 'FingerprintAccessor::Get()'
grep -A4 'int LocalDOMWindow::screenY() const' "${screen_patch}" | grep -q 'FingerprintAccessor::Get()'
grep -A4 'bool Screen::isExtended() const' "${screen_patch}" | grep -q 'FingerprintAccessor::Get()'
grep -q 'third_party/blink/renderer/modules/screen_details/screen_detailed.cc' "${screen_patch}"
grep -q 'third_party/blink/renderer/modules/screen_details/screen_details.cc' "${screen_patch}"
grep -A4 'int ScreenDetailed::left() const' "${screen_patch}" | grep -q 'FingerprintAccessor::Get()'
grep -A4 'int ScreenDetailed::top() const' "${screen_patch}" | grep -q 'FingerprintAccessor::Get()'
grep -A6 'bool ScreenDetailed::isPrimary() const' "${screen_patch}" | grep -q 'return true'
grep -A7 'bool ScreenDetailed::isInternal() const' "${screen_patch}" | grep -q 'return false'
grep -A5 'float ScreenDetailed::devicePixelRatio() const' "${screen_patch}" | grep -q 'fp->screen.pixel_ratio'
grep -A7 'String ScreenDetailed::label() const' "${screen_patch}" | grep -q 'return String()'
grep -A5 'float ScreenDetailed::hdrHeadroom() const' "${screen_patch}" | grep -q 'return 0.f'
grep -A5 'float ScreenDetailed::highDynamicRangeHeadroom() const' "${screen_patch}" | grep -q 'return 1.f'
grep -A5 'float ScreenDetailed::redPrimaryX() const' "${screen_patch}" | grep -q 'kSrgbRedPrimaryX'
grep -A5 'float ScreenDetailed::whitePointY() const' "${screen_patch}" | grep -q 'kSrgbWhitePointY'
grep -q 'const bool protect_topology' "${screen_patch}"
grep -q 'display_id == new_infos.current_display_id' "${screen_patch}"
grep -q 'dispatch_events && !protect_topology' "${screen_patch}"
grep -q 'third_party/blink/renderer/modules/screen_orientation/screen_orientation_controller.cc' "${screen_patch}"
grep -A12 'void ScreenOrientationController::UpdateOrientation()' "${screen_patch}" | grep -q 'fp->screen.height >= fp->screen.width'
grep -A12 'void ScreenOrientationController::UpdateOrientation()' "${screen_patch}" | grep -q 'orientation_->SetAngle(0)'
grep -A5 'void ScreenOrientationController::NotifyOrientationChanged()' "${screen_patch}" | grep -q 'FingerprintAccessor::Get()'
grep -A5 'int LocalDOMWindow::orientation() const' "${screen_patch}" | grep -q 'return 0'
grep -q 'if (fp && !command_line->HasSwitch("window-position"))' "${startup_source}"
grep -q "check('screen.availLeft', 0, screen.availLeft)" "${verify_script}"
grep -q "check('window.screenX', 0, window.screenX)" "${verify_script}"
grep -q "check('screen.orientation.type', expectedOrientation, screen.orientation.type)" "${verify_script}"
grep -q "check('screen.orientation.angle', 0, screen.orientation.angle)" "${verify_script}"
grep -q 'fp->screen.width' "${css_screen_patch}"
grep -A5 'bool MediaValues::CalculateDeviceSupportsHDR' "${css_screen_patch}" | grep -q 'return false'
grep -A5 'ColorSpaceGamut MediaValues::CalculateColorGamut' "${css_screen_patch}" | grep -q 'ColorSpaceGamut::SRGB'
grep -q 'ShouldFilterLocalFonts' "${font_enum_patch}"
grep -q 'IsLocalFontBlocked' "${css_font_patch}"
grep -q 'IsLocalFontBlocked' "${font_fallback_patch}"
