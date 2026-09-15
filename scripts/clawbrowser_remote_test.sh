#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
REMOTE_SCRIPT="${REPO_ROOT}/scripts/clawbrowser_remote.sh"

fail() {
  printf 'FAIL: %s\n' "$*" >&2
  exit 1
}

assert_equals() {
  local actual="$1"
  local expected="$2"
  if [[ "${actual}" != "${expected}" ]]; then
    fail "expected '${expected}', got '${actual}'"
  fi
}

load_remote_functions() {
  # Drop the main invocation so the helper functions can be sourced safely.
  local temp_script
  temp_script="$(mktemp)"
  trap 'rm -f "${temp_script}"' RETURN
  sed '/^main "\$@"$/d' "${REMOTE_SCRIPT}" >"${temp_script}"
  # shellcheck disable=SC1090
  source "${temp_script}"
}

run_resolution() {
  local build_dir="$1"
  (
    BUILD_DIR="${build_dir}"
    load_remote_functions
    integration_browser_binary
  )
}

test_prefers_existing_clawbrowser_bundle() {
  local tmp_dir
  tmp_dir="$(mktemp -d)"
  mkdir -p "${tmp_dir}/Clawbrowser.app/Contents/MacOS"
  : >"${tmp_dir}/Clawbrowser.app/Contents/MacOS/Clawbrowser"
  chmod +x "${tmp_dir}/Clawbrowser.app/Contents/MacOS/Clawbrowser"

  local resolved
  resolved="$(run_resolution "${tmp_dir}")"
  assert_equals "${resolved}" \
    "${tmp_dir}/Clawbrowser.app/Contents/MacOS/Clawbrowser"

  rm -rf "${tmp_dir}"
}

test_errors_when_only_chromium_bundle_exists() {
  local tmp_dir
  tmp_dir="$(mktemp -d)"
  mkdir -p "${tmp_dir}/Chromium.app/Contents/MacOS"
  : >"${tmp_dir}/Chromium.app/Contents/MacOS/Chromium"
  chmod +x "${tmp_dir}/Chromium.app/Contents/MacOS/Chromium"

  local output
  set +e
  output="$(run_resolution "${tmp_dir}" 2>&1)"
  local status=$?
  set -e

  if [[ "${status}" == "0" ]]; then
    fail "expected Chromium bundle to be rejected"
  fi
  case "${output}" in
    *"Built browser binary not found under ${tmp_dir}"*) ;;
    *)
      fail "expected missing-binary error, got: ${output}"
      ;;
  esac

  rm -rf "${tmp_dir}"
}

test_supports_clawbrowser_linux_binary() {
  local tmp_dir
  tmp_dir="$(mktemp -d)"
  : >"${tmp_dir}/clawbrowser"
  chmod +x "${tmp_dir}/clawbrowser"

  local resolved
  resolved="$(run_resolution "${tmp_dir}")"
  assert_equals "${resolved}" "${tmp_dir}/clawbrowser"

  rm -rf "${tmp_dir}"
}

test_errors_when_only_chrome_binary_exists() {
  local tmp_dir
  tmp_dir="$(mktemp -d)"
  : >"${tmp_dir}/chrome"
  chmod +x "${tmp_dir}/chrome"

  local output
  set +e
  output="$(run_resolution "${tmp_dir}" 2>&1)"
  local status=$?
  set -e

  if [[ "${status}" == "0" ]]; then
    fail "expected chrome binary to be rejected"
  fi
  case "${output}" in
    *"Built browser binary not found under ${tmp_dir}"*) ;;
    *)
      fail "expected missing-binary error, got: ${output}"
      ;;
  esac

  rm -rf "${tmp_dir}"
}

test_stages_dev_macos_clawbrowser_bundle() {
  local tmp_dir
  tmp_dir="$(mktemp -d)"
  local project_dir="${tmp_dir}/project"
  local build_dir="${tmp_dir}/out/CBFast"
  local icon_dir="${project_dir}/branding/icons/app/side-bite"
  local source_app="${build_dir}/Chromium.app"
  local staged_app="${build_dir}/Clawbrowser.app"
  mkdir -p "${icon_dir}" "${source_app}/Contents/MacOS" \
           "${source_app}/Contents/Frameworks" \
           "${source_app}/Contents/Resources" \
           "${tmp_dir}/bin"
  : >"${icon_dir}/app.icns"
  : >"${icon_dir}/product_logo_32.png"
  : >"${source_app}/Contents/MacOS/Chromium"
  : >"${build_dir}/libc++_chrome.dylib"
  printf 'old chromium app asset catalog\n' >"${source_app}/Contents/Resources/Assets.car"
  chmod +x "${source_app}/Contents/MacOS/Chromium"
  python3 - "${source_app}/Contents/Info.plist" <<'PY'
import plistlib
import sys

with open(sys.argv[1], "wb") as handle:
    plistlib.dump({
        "CFBundleExecutable": "Chromium",
        "CFBundleName": "Chromium",
        "CFBundleIdentifier": "org.chromium.Chromium",
        "CFBundleIconName": "AppIcon",
        "CFBundleShortVersionString": "148.0.7769.0",
        "CFBundleVersion": "7769.0",
        "CFBundleURLTypes": [{
            "CFBundleURLName": "Direct launch URL",
            "CFBundleURLSchemes": ["chromium"],
        }],
    }, handle)
PY
  cat >"${tmp_dir}/bin/codesign" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
target="${@: -1}"
printf 'codesign %s\n' "$*" >"${target}/codesign-invocations.txt"
EOF
  chmod +x "${tmp_dir}/bin/codesign"

  (
    PATH="${tmp_dir}/bin:${PATH}"
    PROJECT_DIR="${project_dir}"
    CHROMIUM_DIR="${tmp_dir}/chromium"
    BUILD_DIR="${build_dir}"
    FINGERPRINT_ID="clawbrowser_default"
    CLAWBROWSER_BUNDLE_VERSION="1.2.3"
    load_remote_functions
    stage_dev_browser_bundle Darwin
  )

  [[ -x "${staged_app}/Contents/MacOS/Clawbrowser" ]] || \
    fail "expected Clawbrowser browser binary to be executable"
  [[ ! -e "${staged_app}/Contents/MacOS/Clawbrowser.real" ]] || \
    fail "expected macOS bundle to avoid Clawbrowser.real wrapper indirection"
  [[ ! -e "${staged_app}/Contents/MacOS/Chromium" ]] || \
    fail "expected Chromium browser binary to be renamed"
  [[ -f "${staged_app}/Contents/Frameworks/libc++_chrome.dylib" ]] || \
    fail "expected libc++_chrome.dylib to be copied into the bundle frameworks"
  [[ -f "${staged_app}/Contents/Resources/app.icns" ]] || \
    fail "expected app.icns to be copied into the bundle"
  [[ ! -e "${staged_app}/Contents/Resources/Assets.car" ]] || \
    fail "expected staged bundle to remove Assets.car so app.icns is authoritative"
  [[ -f "${staged_app}/codesign-invocations.txt" ]] || \
    fail "expected staged macOS bundle to be re-signed"
  grep -Fq -- 'codesign --force --deep --sign -' \
    "${staged_app}/codesign-invocations.txt" || \
    fail "expected staged macOS bundle to be signed with ad-hoc bundle signature"

  local plist_summary
  plist_summary="$(
    python3 - "${staged_app}/Contents/Info.plist" <<'PY'
import plistlib
import sys

with open(sys.argv[1], "rb") as handle:
    data = plistlib.load(handle)
print(data.get("CFBundleExecutable"))
print(data.get("CFBundleName"))
print(data.get("CFBundleIdentifier"))
print(data.get("CFBundleIconFile"))
print(data.get("CFBundleIconName"))
print(data.get("CFBundleShortVersionString"))
print(data.get("CFBundleVersion"))
print(data.get("CFBundleURLTypes")[0].get("CFBundleURLSchemes")[0])
print(data.get("LSEnvironment", {}).get("CLAWBROWSER_DEFAULT_FINGERPRINT_ID"))
PY
  )"
  case "${plist_summary}" in
    $'Clawbrowser\nClawbrowser\nai.clawbrowser.Clawbrowser\napp\nNone\n1.2.3\n1.2.3\nclawbrowser\nNone') ;;
    *) fail "unexpected Info.plist summary: ${plist_summary}" ;;
  esac

  rm -rf "${tmp_dir}"
}

test_stages_dev_macos_notification_helper_branding() {
  local tmp_dir
  tmp_dir="$(mktemp -d)"
  local project_dir="${tmp_dir}/project"
  local build_dir="${tmp_dir}/out/CBFast"
  local icon_dir="${project_dir}/branding/icons/app/side-bite"
  local source_app="${build_dir}/Chromium.app"
  local staged_app="${build_dir}/Clawbrowser.app"
  local helper_dir="${source_app}/Contents/Frameworks/Chromium Helper (Alerts).app"
  mkdir -p "${icon_dir}" "${source_app}/Contents/MacOS" \
           "${source_app}/Contents/Frameworks" \
           "${source_app}/Contents/Resources" \
           "${helper_dir}/Contents/MacOS" \
           "${tmp_dir}/bin"
  : >"${icon_dir}/app.icns"
  : >"${icon_dir}/product_logo_32.png"
  : >"${source_app}/Contents/MacOS/Chromium"
  : >"${build_dir}/libc++_chrome.dylib"
  : >"${helper_dir}/Contents/MacOS/Chromium Helper (Alerts)"
  chmod +x "${source_app}/Contents/MacOS/Chromium" \
           "${helper_dir}/Contents/MacOS/Chromium Helper (Alerts)"
  python3 - "${source_app}/Contents/Info.plist" \
             "${helper_dir}/Contents/Info.plist" <<'PY'
import plistlib
import sys

with open(sys.argv[1], "wb") as handle:
    plistlib.dump({
        "CFBundleExecutable": "Chromium",
        "CFBundleName": "Chromium",
        "CFBundleIdentifier": "org.chromium.Chromium",
    }, handle)

with open(sys.argv[2], "wb") as handle:
    plistlib.dump({
        "CFBundleExecutable": "Chromium Helper (Alerts)",
        "CFBundleName": "Chromium Helper (Alerts)",
        "CFBundleDisplayName": "Chromium Helper (Alerts)",
        "CFBundleIdentifier": "org.chromium.Chromium.helper.alerts",
    }, handle)
PY
  cat >"${tmp_dir}/bin/codesign" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
target="${@: -1}"
printf 'codesign %s\n' "$*" >"${target}/codesign-invocations.txt"
EOF
  chmod +x "${tmp_dir}/bin/codesign"

  (
    PATH="${tmp_dir}/bin:${PATH}"
    PROJECT_DIR="${project_dir}"
    CHROMIUM_DIR="${tmp_dir}/chromium"
    BUILD_DIR="${build_dir}"
    FINGERPRINT_ID="clawbrowser_default"
    load_remote_functions
    stage_dev_browser_bundle Darwin
  )

  local helper_summary
  helper_summary="$(
    python3 - "${staged_app}/Contents/Frameworks/Chromium Helper (Alerts).app/Contents/Info.plist" <<'PY'
import plistlib
import sys

with open(sys.argv[1], "rb") as handle:
    data = plistlib.load(handle)
print(data.get("CFBundleExecutable"))
print(data.get("CFBundleName"))
print(data.get("CFBundleDisplayName"))
print(data.get("CFBundleIdentifier"))
PY
  )"
  case "${helper_summary}" in
    $'Chromium Helper (Alerts)\nClawbrowser Helper (Alerts)\nClawbrowser Helper (Alerts)\norg.chromium.Chromium.helper.alerts') ;;
    *) fail "unexpected helper Info.plist summary: ${helper_summary}" ;;
  esac

  rm -rf "${tmp_dir}"
}

test_stages_dev_linux_clawbrowser_bundle() {
  local tmp_dir
  tmp_dir="$(mktemp -d)"
  local project_dir="${tmp_dir}/project"
  local icon_dir="${project_dir}/branding/icons/app/side-bite"
  mkdir -p "${icon_dir}" "${tmp_dir}/resources"
  : >"${tmp_dir}/chrome"
  chmod +x "${tmp_dir}/chrome"
  for size in 16 22 24 32 48 64 128 256 512 1024; do
    printf 'icon-%s\n' "${size}" >"${icon_dir}/product_logo_${size}.png"
  done

  (
    BUILD_DIR="${tmp_dir}"
    PROJECT_DIR="${project_dir}"
    FINGERPRINT_ID="clawbrowser_default"
    load_remote_functions
    stage_dev_browser_bundle Linux
  )

  [[ -x "${tmp_dir}/clawbrowser" ]] || \
    fail "expected Linux clawbrowser wrapper to be executable"
  [[ -x "${tmp_dir}/clawbrowser.real" ]] || \
    fail "expected Linux staging to create clawbrowser.real"
  [[ ! -e "${tmp_dir}/chrome" ]] || \
    fail "expected Linux staging to move chrome into clawbrowser.real"
  grep -Fq -- 'exec "${SELF_DIR}/clawbrowser.real" --disable-features=DialMediaRouteProvider "$@"' \
    "${tmp_dir}/clawbrowser" || \
    fail "expected Linux wrapper to match the shipped prod launcher"
  grep -Fq -- '--fingerprint=' "${tmp_dir}/clawbrowser" && \
    fail "expected Linux wrapper not to force a fingerprint profile"
  grep -Fq -- '--regenerate' "${tmp_dir}/clawbrowser" && \
    fail "expected Linux wrapper not to force regeneration"
  [[ -f "${tmp_dir}/product_logo_256.png" ]] || \
    fail "expected Linux staging to copy desktop icons into the bundle root"
  [[ -f "${tmp_dir}/resources/product_logo_256.png" ]] || \
    fail "expected Linux staging to copy desktop icons into resources/"

  rm -rf "${tmp_dir}"
}

test_linux_patch_prereqs_installs_appimagetool() {
  local tmp_dir
  tmp_dir="$(mktemp -d)"
  local record="${tmp_dir}/record.txt"
  mkdir -p "${tmp_dir}/bin" "${tmp_dir}/appimagetool"

  cat >"${tmp_dir}/bin/sudo" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
printf 'sudo %s\n' "$*" >>"${FAKE_RECORD:?}"
exit 0
EOF
  chmod +x "${tmp_dir}/bin/sudo"

  (
    PATH="${tmp_dir}/bin:${PATH}"
    export FAKE_RECORD="${record}"
    APPIMAGETOOL_DIR="${tmp_dir}/appimagetool"
    APPIMAGETOOL_BIN="${tmp_dir}/appimagetool/appimagetool"
    uname() {
      case "$1" in
        -s)
          printf 'Linux\n'
          ;;
        -m)
          printf 'x86_64\n'
          ;;
        *)
          command uname "$@"
          ;;
      esac
    }
    load_remote_functions
    command_exists() {
      case "$1" in
        rsvg-convert|appimagetool)
          return 1
          ;;
        *)
          command -v "$1" >/dev/null 2>&1
          ;;
      esac
    }
    download_appimagetool_appimage() {
      printf 'download_appimagetool_appimage %s %s\n' "$1" "$2" >>"${FAKE_RECORD:?}"
      mkdir -p "$(dirname "$1")"
      : >"$1"
      chmod 0755 "$1"
    }
    ensure_patch_prereqs
  )

  grep -Fq -- 'sudo apt-get update' "${record}" || \
    fail "expected Linux prereqs to update apt before installing librsvg2-bin"
  grep -Fq -- 'sudo apt-get install -y librsvg2-bin' "${record}" || \
    fail "expected Linux prereqs to install librsvg2-bin"
  grep -Fq -- 'download_appimagetool_appimage ' "${record}" || \
    fail "expected Linux prereqs to download appimagetool when missing"
  [[ -x "${tmp_dir}/appimagetool/appimagetool" ]] || \
    fail "expected Linux prereqs to create a reusable appimagetool wrapper"

  rm -rf "${tmp_dir}"
}

test_stages_dev_macos_bundle_resigns_after_rewrite() {
  local tmp_dir
  tmp_dir="$(mktemp -d)"
  local project_dir="${tmp_dir}/project"
  local build_dir="${tmp_dir}/out/CBFast"
  local icon_dir="${project_dir}/branding/icons/app/side-bite"
  local source_app="${build_dir}/Chromium.app"
  local staged_app="${build_dir}/Clawbrowser.app"
  mkdir -p "${icon_dir}" "${source_app}/Contents/MacOS" \
           "${source_app}/Contents/Frameworks" \
           "${source_app}/Contents/Resources" \
           "${tmp_dir}/bin"
  : >"${icon_dir}/app.icns"
  : >"${icon_dir}/product_logo_32.png"
  : >"${source_app}/Contents/MacOS/Chromium"
  : >"${build_dir}/libc++_chrome.dylib"
  chmod +x "${source_app}/Contents/MacOS/Chromium"
  python3 - "${source_app}/Contents/Info.plist" <<'PY'
import plistlib
import sys

with open(sys.argv[1], "wb") as handle:
    plistlib.dump({
        "CFBundleExecutable": "Chromium",
        "CFBundleName": "Chromium",
        "CFBundleIdentifier": "org.chromium.Chromium",
    }, handle)
PY
  cat >"${tmp_dir}/bin/codesign" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
target="${@: -1}"
printf 'codesign %s\n' "$*" >"${target}/codesign-invocations.txt"
EOF
  chmod +x "${tmp_dir}/bin/codesign"

  (
    PATH="${tmp_dir}/bin:${PATH}"
    PROJECT_DIR="${project_dir}"
    CHROMIUM_DIR="${tmp_dir}/chromium"
    BUILD_DIR="${build_dir}"
    FINGERPRINT_ID="clawbrowser_default"
    load_remote_functions
    stage_dev_browser_bundle Darwin
  )

  [[ -f "${staged_app}/codesign-invocations.txt" ]] || \
    fail "expected staged macOS bundle to be re-signed"
  grep -Fq -- 'codesign --force --deep --sign -' \
    "${staged_app}/codesign-invocations.txt" || \
    fail "expected ad-hoc codesign invocation, got: $(cat "${staged_app}/codesign-invocations.txt")"

  rm -rf "${tmp_dir}"
}

test_reset_patch_targets_restores_nested_v8_paths() {
  local tmp_dir
  tmp_dir="$(mktemp -d)"
  local chromium_dir="${tmp_dir}/chromium"
  local src_dir="${chromium_dir}/src"
  local v8_dir="${src_dir}/v8"
  local project_dir="${tmp_dir}/project"
  mkdir -p "${src_dir}" "${v8_dir}/src/inspector" \
           "${project_dir}/clawbrowser/patches"

  git -C "${src_dir}" init -q
  git -C "${src_dir}" config user.email test@example.com
  git -C "${src_dir}" config user.name Test
  : >"${src_dir}/README.md"
  git -C "${src_dir}" add README.md
  git -C "${src_dir}" commit -q -m init

  git -C "${v8_dir}" init -q
  git -C "${v8_dir}" config user.email test@example.com
  git -C "${v8_dir}" config user.name Test
  printf 'clean\n' >"${v8_dir}/src/inspector/value-mirror.cc"
  git -C "${v8_dir}" add src/inspector/value-mirror.cc
  git -C "${v8_dir}" commit -q -m init

  cat >"${project_dir}/clawbrowser/patches/033-v8.patch" <<'EOF'
diff --git a/v8/src/inspector/value-mirror.cc b/v8/src/inspector/value-mirror.cc
--- a/v8/src/inspector/value-mirror.cc
+++ b/v8/src/inspector/value-mirror.cc
@@ -1 +1 @@
-clean
+patched
EOF
  printf 'dirty\n' >"${v8_dir}/src/inspector/value-mirror.cc"

  (
    CHROMIUM_DIR="${chromium_dir}"
    CHROMIUM_REVISION="$(git -C "${src_dir}" rev-parse HEAD)"
    CHROMIUM_VERSION_LABEL="test"
    PROJECT_DIR="${project_dir}"
    load_remote_functions
    reset_patch_targets_to_pin
  )

  local restored
  restored="$(cat "${v8_dir}/src/inspector/value-mirror.cc")"
  assert_equals "${restored}" "clean"

  rm -rf "${tmp_dir}"
}

test_reset_patch_targets_restores_nested_skia_paths() {
  local tmp_dir
  tmp_dir="$(mktemp -d)"
  local chromium_dir="${tmp_dir}/chromium"
  local src_dir="${chromium_dir}/src"
  local skia_dir="${src_dir}/third_party/skia"
  local project_dir="${tmp_dir}/project"
  mkdir -p "${skia_dir}/src/ports" "${project_dir}/clawbrowser/patches"
  git -C "${src_dir}" init -q
  git -C "${src_dir}" -c user.email=test@example.com -c user.name=Test commit --allow-empty -q -m init
  git -C "${skia_dir}" init -q
  printf 'clean\n' >"${skia_dir}/src/ports/SkTypeface_fontations.cpp"
  git -C "${skia_dir}" add src/ports/SkTypeface_fontations.cpp
  git -C "${skia_dir}" -c user.email=test@example.com -c user.name=Test commit -q -m init
  cat >"${project_dir}/clawbrowser/patches/040-skia.patch" <<'EOF'
--- a/third_party/skia/src/ports/SkTypeface_fontations.cpp
+++ b/third_party/skia/src/ports/SkTypeface_fontations.cpp
@@ -1 +1 @@
-clean
+patched
EOF
  cat >"${project_dir}/clawbrowser/patches/048-added.patch" <<'EOF'
--- /dev/null
+++ b/third_party/skia/src/ports/ClawbrowserAdded.h
@@ -0,0 +1 @@
+owned
EOF
  local helper
  for helper in clawbrowser_remote.sh chromium_remote.sh; do
    printf 'dirty\n' >"${skia_dir}/src/ports/SkTypeface_fontations.cpp"
    printf 'owned\n' >"${skia_dir}/src/ports/ClawbrowserAdded.h"
    printf 'unrelated\n' >"${skia_dir}/src/ports/Unrelated.h"
    (
      CHROMIUM_DIR="${chromium_dir}"
      CHROMIUM_REVISION="$(git -C "${src_dir}" rev-parse HEAD)"
      CHROMIUM_VERSION_LABEL="test"
      PROJECT_DIR="${project_dir}"
      REMOTE_SCRIPT="${REPO_ROOT}/scripts/${helper}"
      load_remote_functions
      reset_patch_targets_to_pin
    )
    assert_equals "$(cat "${skia_dir}/src/ports/SkTypeface_fontations.cpp")" "clean"
    [[ ! -e "${skia_dir}/src/ports/ClawbrowserAdded.h" ]] || fail "patch-created Skia header was not reset"
    assert_equals "$(cat "${skia_dir}/src/ports/Unrelated.h")" "unrelated"
    # An absent upstream target not explicitly created by a patch must fail,
    # not silently delete an unrelated untracked file with the same name.
    cat >"${project_dir}/clawbrowser/patches/099-invalid.patch" <<'EOF'
--- a/third_party/skia/src/ports/Unrelated.h
+++ b/third_party/skia/src/ports/Unrelated.h
@@ -1 +1 @@
-unexpected
+replacement
EOF
    if (
      CHROMIUM_DIR="${chromium_dir}"
      CHROMIUM_REVISION="$(git -C "${src_dir}" rev-parse HEAD)"
      CHROMIUM_VERSION_LABEL="test"
      PROJECT_DIR="${project_dir}"
      REMOTE_SCRIPT="${REPO_ROOT}/scripts/${helper}"
      load_remote_functions
      reset_patch_targets_to_pin
    ) >/dev/null 2>&1; then
      fail "unexpected absent Skia target should be rejected"
    fi
    assert_equals "$(cat "${skia_dir}/src/ports/Unrelated.h")" "unrelated"
    rm -f "${project_dir}/clawbrowser/patches/099-invalid.patch"
  done
  rm -rf "${tmp_dir}"
}

test_collect_patch_targets_includes_legacy_cleanup_targets() {
  local tmp_dir
  tmp_dir="$(mktemp -d)"
  local project_dir="${tmp_dir}/project"
  mkdir -p "${project_dir}/clawbrowser/patches"

  cat >"${project_dir}/clawbrowser/patches/001-current.patch" <<'EOF'
diff --git a/current/file.cc b/current/file.cc
--- a/current/file.cc
+++ b/current/file.cc
@@ -1 +1 @@
-old
+new
EOF
  cat >"${project_dir}/clawbrowser/patches/legacy_cleanup_targets.txt" <<'EOF'
# Targets from removed historical patches that must be reset before applying
# the current patch set.
stale/old_patch.cc
EOF

  local targets
  targets="$(
    PROJECT_DIR="${project_dir}"
    load_remote_functions
    collect_patch_targets
  )"

  case "${targets}" in
    *$'current/file.cc'*$'stale/old_patch.cc'*)
      ;;
    *)
      fail "expected current and legacy cleanup targets, got: ${targets}"
      ;;
  esac

  rm -rf "${tmp_dir}"
}

test_prefers_existing_clawbrowser_bundle
test_errors_when_only_chromium_bundle_exists
test_supports_clawbrowser_linux_binary
test_errors_when_only_chrome_binary_exists
test_stages_dev_macos_clawbrowser_bundle
test_stages_dev_macos_notification_helper_branding
test_stages_dev_macos_bundle_resigns_after_rewrite
test_stages_dev_linux_clawbrowser_bundle
test_linux_patch_prereqs_installs_appimagetool
test_reset_patch_targets_restores_nested_v8_paths
test_reset_patch_targets_restores_nested_skia_paths
test_collect_patch_targets_includes_legacy_cleanup_targets

printf 'PASS\n'
