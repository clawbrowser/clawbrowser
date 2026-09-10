#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

webgl_patch="${repo_root}/clawbrowser/patches/008-webgl-override.patch"
canvas_patch="${repo_root}/clawbrowser/patches/007-canvas-noise.patch"
canvas_helper="${repo_root}/clawbrowser/noise/canvas_noise.h"
build_file="${repo_root}/clawbrowser/BUILD.gn"
startup_source="${repo_root}/clawbrowser/startup.cc"
args_header="${repo_root}/clawbrowser/cli/args.h"
cdp_patch="${repo_root}/clawbrowser/patches/033-devtools-no-getter-preview.patch"
renderer_loader_patch="${repo_root}/clawbrowser/patches/002-renderer-main-loader.patch"
gpu_loader_patch="${repo_root}/clawbrowser/patches/003-gpu-main-loader.patch"
child_switch_patch="${repo_root}/clawbrowser/patches/004-child-process-flag-propagation.patch"
loader_source="${repo_root}/clawbrowser/fingerprint_loader.cc"
screen_patch="${repo_root}/clawbrowser/patches/006-screen-metrics.patch"
css_screen_patch="${repo_root}/clawbrowser/patches/036-css-media-screen.patch"
font_enum_patch="${repo_root}/clawbrowser/patches/011-fonts-filter.patch"
css_font_patch="${repo_root}/clawbrowser/patches/034-css-font-probing.patch"
font_fallback_patch="${repo_root}/clawbrowser/patches/035-font-fallback-probing.patch"
verify_script="${repo_root}/clawbrowser/verify/resources/verify.js"
verify_html="${repo_root}/clawbrowser/verify/resources/verify.html"
verify_source="${repo_root}/clawbrowser/verify/verify_page.cc"

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
grep -q 'AppendSwitchASCII("use-webgpu-adapter", "swiftshader")' "${startup_source}"
grep -q 'request->runtime_gpu = RuntimeGPUHint(command_line)' "${startup_source}"
grep -q 'policy.canvas != "override"' "${startup_source}"
grep -q 'policy.fonts != "native_or_allowlist"' "${startup_source}"
grep -q 'request->runtime_browser_version = version_info::GetVersionNumber()' "${startup_source}"
grep -q 'request->runtime_os_version.reset()' "${startup_source}"
grep -q '!IsSwiftShaderWebGLBackend' "${startup_source}"

# Canvas readback and export must share coordinate-stable, RGB-only noise.
grep -q 'base_rendering_context_2d.cc' "${canvas_patch}"
[[ "$(grep -c 'ApplyFingerprintNoise(' "${canvas_patch}")" -ge 3 ]]
grep -q 'std::min<int64_t>(sx' "${canvas_patch}"
grep -q 'CreateForCanvas(image_bitmap)' "${canvas_patch}"
grep -q 'canvas_async_blob_creator.cc' "${canvas_patch}"
grep -q 'std::move(source_buffer_)' "${canvas_patch}"
grep -q 'ApplyCanvasFingerprintNoise' "${canvas_patch}"
if grep -q 'canvas_rendering_context_2d.cc' "${canvas_patch}"; then
  echo "Canvas noise must hook the shared BaseRenderingContext2D path." >&2
  exit 1
fi
grep -q 'logical_channel < 3' "${canvas_helper}"
grep -q 'pixel + (2 - logical_channel)' "${canvas_helper}"
grep -q 'PixelNoiseSeed\|PixelSeed' "${canvas_helper}"
grep -q 'CanonicalizeUint8Lsb' "${canvas_helper}"
grep -q 'CanonicalizeFloat16Lsb' "${canvas_helper}"
grep -q 'CanonicalizeFloat32Lsb' "${canvas_helper}"
grep -q 'NextUint32() & 1u' "${canvas_helper}"
grep -q 'kExponentMask' "${canvas_helper}"
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
grep -q 'kRequireProxySwitch\[\].*clawbrowser-require-proxy' "${args_header}"
grep -q 'conflicting_proxy_switch' "${startup_source}"
grep -q 'ConflictingRequiredProxySwitch' "${startup_source}"
grep -q 'args.require_proxy() && !proxy' "${startup_source}"
grep -q 'proxy_flags.empty()' "${startup_source}"
grep -q 'required_proxy_missing' "${startup_source}"
grep -q 'invalid_proxy_config' "${startup_source}"
grep -q 'ManagedProxyPrivacyCapabilityForCommandLine' "${verify_source}"
grep -q 'data-managed-proxy-privacy="\$i18n{managed_proxy_privacy}"' \
  "${verify_html}"
grep -q 'capabilityData.dataset.managedProxyPrivacy' "${verify_script}"
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
grep -q 'third_party/blink/renderer/core/events/mouse_event.h' "${screen_patch}"
grep -q 'third_party/blink/renderer/core/input/touch_event_manager.cc' "${screen_patch}"
grep -A35 'gfx::PointF EventScreenPositionForFingerprint' "${screen_patch}" |
  grep -q 'BlinkSpaceToDIPs'
grep -A20 'void MouseEvent::SetCoordinatesFromWebPointerProperties' "${screen_patch}" |
  grep -q 'EventScreenPositionForFingerprint'
grep -A20 'Touch\* TouchEventManager::CreateDomTouch' "${screen_patch}" |
  grep -q 'EventScreenPositionForFingerprint'
grep -A18 'int LocalDOMWindow::outerHeight() const' "${screen_patch}" |
  grep -q 'DeriveWindowSize'
grep -A18 'int LocalDOMWindow::outerWidth() const' "${screen_patch}" |
  grep -q 'DeriveWindowSize'
grep -q 'third_party/blink/renderer/core/resize_observer/resize_observer_utilities.cc' "${screen_patch}"
grep -A40 'ComputeSnappedDevicePixelContentBox(' "${screen_patch}" |
  grep -q 'fp->screen.pixel_ratio / native_dpr'
grep -q 'third_party/blink/renderer/modules/csspaint/paint_worklet_global_scope.cc' "${screen_patch}"
grep -A7 'double PaintWorkletGlobalScope::devicePixelRatio() const' "${screen_patch}" |
  grep -q 'fp->screen.pixel_ratio'
grep -A12 'component("clawbrowser_runtime")' "${build_file}" |
  grep -q 'fingerprint_coherence.h'
grep -q 'if (fp && !command_line->HasSwitch("window-position"))' "${startup_source}"
grep -q "check('screen.availLeft', 0, screen.availLeft)" "${verify_script}"
grep -q "check('window.screenX', 0, window.screenX)" "${verify_script}"
grep -q "check('screen.orientation.type', expectedOrientation, screen.orientation.type)" "${verify_script}"
grep -q "check('screen.orientation.angle', 0, screen.orientation.angle)" "${verify_script}"
grep -q 'fp->screen.width' "${css_screen_patch}"
grep -A5 'bool MediaValues::CalculateDeviceSupportsHDR' "${css_screen_patch}" | grep -q 'return false'
grep -A5 'ColorSpaceGamut MediaValues::CalculateColorGamut' "${css_screen_patch}" | grep -q 'ColorSpaceGamut::SRGB'
grep -A6 'int MediaValues::CalculateColorBitsPerComponent' "${css_screen_patch}" | grep -q 'fp->screen.color_depth'
grep -A5 'int MediaValues::CalculateMonochromeBitsPerComponent' "${css_screen_patch}" | grep -q 'return 0'
grep -q 'ShouldFilterLocalFonts' "${font_enum_patch}"
grep -q 'FontMetadata.blob()' "${font_enum_patch}"
grep -A12 'ShouldFilterLocalFonts' "${font_enum_patch}" |
  grep -q 'resolver->Resolve(std::move(entries))'
if grep -q 'element.full_name(), element.family()' "${font_enum_patch}"; then
  echo "Font enumeration must not admit every face solely by family name." >&2
  exit 1
fi
grep -q 'IsLocalFontBlocked' "${css_font_patch}"
grep -q 'css_font_selector_base.cc' "${css_font_patch}"
grep -q 'offscreen_font_selector.cc' "${css_font_patch}"
grep -q 'local_font_face_source.cc' "${css_font_patch}"
grep -A8 'bool LocalFontFaceSource::IsLocalFontAvailable' "${css_font_patch}" |
  grep -q 'IsLocalFontBlocked'
grep -q 'IsLocalFontBlocked' "${font_fallback_patch}"
grep -A4 'const bool clawbrowser_blocked' "${font_fallback_patch}" |
  grep -q 'FamilyIsGeneric'
grep -A10 'void CSSFontSelectorBase::WillUseFontData' "${css_font_patch}" |
  grep -q 'IsLocalFontBlocked'
grep -q 'ValidateFingerprintForRuntime' "${loader_source}"
grep -q 'protected font allowlist contains an empty' "${loader_source}"

echo "PASS"
