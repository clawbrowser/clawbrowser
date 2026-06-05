#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

SCENARIO_TMP_DIR=""
SCENARIO_HOME=""
SCENARIO_OUTPUT=""
SCENARIO_STATUS=0
SCENARIO_RECORD=""
SCENARIO_RSYNC_RECORD=""
SCENARIO_RUNNER_BODY=""

fail() {
  printf 'FAIL: %s\n' "$*" >&2
  exit 1
}

assert_contains() {
  local haystack="$1"
  local needle="$2"
  if [[ "${haystack}" != *"${needle}"* ]]; then
    fail "expected to find '${needle}' in output:
${haystack}"
  fi
}

assert_not_contains() {
  local haystack="$1"
  local needle="$2"
  if [[ "${haystack}" == *"${needle}"* ]]; then
    fail "expected not to find '${needle}' in output:
${haystack}"
  fi
}

assert_equals() {
  local actual="$1"
  local expected="$2"
  if [[ "${actual}" != "${expected}" ]]; then
    fail "expected '${expected}', got '${actual}'"
  fi
}

cleanup_scenario() {
  if [[ -n "${SCENARIO_TMP_DIR}" && -d "${SCENARIO_TMP_DIR}" ]]; then
    rm -rf "${SCENARIO_TMP_DIR}"
  fi
  SCENARIO_TMP_DIR=""
  SCENARIO_HOME=""
  SCENARIO_OUTPUT=""
  SCENARIO_STATUS=0
  SCENARIO_RECORD=""
  SCENARIO_RSYNC_RECORD=""
  SCENARIO_RUNNER_BODY=""
}

run_launcher() {
  local remote_os="$1"
  local behavior="$2"
  local skip_rsync="${3:-1}"
  local appimage_release_name="${4:-}"
  local tmp_dir
  local remote_home
  local -a launcher_args=()

  cleanup_scenario

  tmp_dir="$(mktemp -d)"
  if [[ "${remote_os}" == "Darwin" ]]; then
    remote_home="${tmp_dir}/Users/remote"
  else
    remote_home="${tmp_dir}/home/remote"
  fi

  mkdir -p "${tmp_dir}/bin" "${remote_home}"
  touch "${tmp_dir}/id_test"

  cat >"${tmp_dir}/bin/ssh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

record_dir="${FAKE_REMOTE_RECORD_DIR:?}"
stdin_payload="$(cat || true)"
payload="${stdin_payload}"
if [[ -z "${payload}" ]]; then
  payload="${!#}"
fi

{
  printf 'ARGS=%s\n' "$*"
  printf 'PAYLOAD<<EOF\n%s\nEOF\n' "${payload}"
  printf -- '---\n'
} >>"${record_dir}/ssh_invocations.txt"

if [[ "${payload}" == *"REMOTE_FACTS_MARKER=1"* ]]; then
  printf 'REMOTE_OS=%s\n' "${FAKE_REMOTE_OS:?}"
  printf 'REMOTE_HOME=%s\n' "${FAKE_REMOTE_HOME:?}"
  exit 0
fi

if [[ "${payload}" != *"ARTIFACT_RUN_MARKER=1"* ]]; then
  printf 'Unhandled ssh payload\n' >&2
  exit 1
fi

runner_b64="${!#}"
python3 -c 'import base64, pathlib, sys; pathlib.Path(sys.argv[2]).write_text(base64.b64decode(sys.argv[1]).decode())' \
  "${runner_b64}" \
  "${record_dir}/runner_body.sh"

log_root="${FAKE_REMOTE_HOME}/dev/clawbrowser-build-logs/prod-artifacts"
artifact_dir="${FAKE_REMOTE_HOME}/dev/clawbrowser-artifacts"
run_dir="${log_root}/prod-artifacts-20260410-200000-CEST-4242"
appimage_release_name="${@: -2:1}"
if [[ "${appimage_release_name}" == "__CLAWBROWSER_EMPTY_REMOTE_ARG__" ]]; then
  appimage_release_name=""
fi
if [[ -n "${appimage_release_name}" ]]; then
  case "${appimage_release_name}" in
    clawbrowser-*)
      appimage_artifact_basename="${appimage_release_name}"
      ;;
    *)
      appimage_artifact_basename="clawbrowser-${appimage_release_name}"
      ;;
  esac
else
  appimage_artifact_basename=""
fi

  case "${FAKE_REMOTE_BEHAVIOR:-success}" in
  success)
    printf 'RUN_DIR=%s\n' "${run_dir}"
    printf 'PID=%s\n' '4242'
    printf 'SUMMARY=%s\n' "${run_dir}/summary.txt"
    printf 'STATUS=%s\n' "${run_dir}/status.env"
    printf 'NOHUP=%s\n' "${run_dir}/nohup.log"
    printf 'META=%s\n' "${run_dir}/launch.env"
    if [[ "${FAKE_REMOTE_OS:?}" == "Darwin" ]]; then
      printf 'ARTIFACT_COUNT=1\n'
      printf 'ARTIFACT_1=%s\n' "${artifact_dir}/clawbrowser-prod-macos-arm64-20260410-200000-CEST.tar.gz"
    else
      if [[ -n "${appimage_release_name}" ]]; then
        printf 'ARTIFACT_COUNT=4\n'
      else
        printf 'ARTIFACT_COUNT=2\n'
      fi
      printf 'ARTIFACT_1=%s\n' "${artifact_dir}/clawbrowser-prod-linux-x64-20260410-200000-CEST.tar.gz"
      printf 'ARTIFACT_2=%s\n' "${artifact_dir}/clawbrowser-prod-linux-arm64-20260410-200000-CEST.tar.gz"
      if [[ -n "${appimage_artifact_basename}" ]]; then
        printf 'ARTIFACT_3=%s\n' "${artifact_dir}/${appimage_artifact_basename}-x64-20260410-200000-CEST.AppImage"
        printf 'ARTIFACT_4=%s\n' "${artifact_dir}/${appimage_artifact_basename}-arm64-20260410-200000-CEST.AppImage"
      fi
    fi
    printf 'LATEST=%s\n' "${log_root}/latest-prod-artifacts"
    ;;
  busy)
    build_root="${FAKE_REMOTE_HOME}/work/chromium/src/out"
    if [[ "${FAKE_REMOTE_OS:?}" == "Darwin" ]]; then
      build_dir_a="${build_root}/CBProdMacArm64"
      build_dir_b="${build_root}/CBProdMacX64"
    else
      build_dir_a="${build_root}/CBProdLinuxX64"
      build_dir_b="${build_root}/CBProdLinuxArm64"
    fi
    printf 'BUILD_DIR_BUSY=1\n'
    printf 'BUILD_DIR_A=%s\n' "${build_dir_a}"
    printf 'BUILD_DIR_B=%s\n' "${build_dir_b}"
    printf 'LOCK_PATH_A=%s\n' "${build_dir_a}.lock"
    printf 'LOCK_PATH_B=%s\n' "${build_dir_b}.lock"
    exit 2
    ;;
  start-fail)
    printf 'FAILED_TO_START=1\n'
    printf 'RUN_DIR=%s\n' "${run_dir}"
    printf 'REASON=%s\n' 'runner did not create start sentinel'
    exit 1
    ;;
  *)
    printf 'Unknown FAKE_REMOTE_BEHAVIOR=%s\n' "${FAKE_REMOTE_BEHAVIOR}" >&2
    exit 1
    ;;
esac
EOF
  chmod +x "${tmp_dir}/bin/ssh"

  cat >"${tmp_dir}/bin/rsync" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
{
  printf 'ARGS=%s\n' "$*"
  printf -- '---\n'
} >>"${FAKE_REMOTE_RECORD_DIR:?}/rsync_invocations.txt"
exit 0
EOF
  chmod +x "${tmp_dir}/bin/rsync"

  if [[ "${skip_rsync}" == "1" ]]; then
    launcher_args+=(--skip-rsync)
  fi

  if [[ -n "${appimage_release_name}" ]]; then
    launcher_args+=(--appimage-release-name "${appimage_release_name}")
  fi

  set +e
  if ((${#launcher_args[@]} > 0)); then
    SCENARIO_OUTPUT="$(
      PATH="${tmp_dir}/bin:${PATH}" \
      FAKE_REMOTE_RECORD_DIR="${tmp_dir}" \
      FAKE_REMOTE_OS="${remote_os}" \
      FAKE_REMOTE_HOME="${remote_home}" \
      FAKE_REMOTE_BEHAVIOR="${behavior}" \
      FINGERPRINT_ID="" \
      SSH_KEY="${tmp_dir}/id_test" \
      REMOTE_HOST="builder@example.com" \
      bash "${REPO_ROOT}/scripts/build_remote_prod_artifacts.sh" "${launcher_args[@]}" 2>&1
    )"
  else
    SCENARIO_OUTPUT="$(
      PATH="${tmp_dir}/bin:${PATH}" \
      FAKE_REMOTE_RECORD_DIR="${tmp_dir}" \
      FAKE_REMOTE_OS="${remote_os}" \
      FAKE_REMOTE_HOME="${remote_home}" \
      FAKE_REMOTE_BEHAVIOR="${behavior}" \
      FINGERPRINT_ID="" \
      SSH_KEY="${tmp_dir}/id_test" \
      REMOTE_HOST="builder@example.com" \
      bash "${REPO_ROOT}/scripts/build_remote_prod_artifacts.sh" 2>&1
    )"
  fi
  SCENARIO_STATUS=$?
  set -e

  SCENARIO_TMP_DIR="${tmp_dir}"
  SCENARIO_HOME="${remote_home}"
  SCENARIO_RECORD="$(cat "${tmp_dir}/ssh_invocations.txt")"
  SCENARIO_RSYNC_RECORD="$(cat "${tmp_dir}/rsync_invocations.txt" 2>/dev/null || true)"
  SCENARIO_RUNNER_BODY="$(cat "${tmp_dir}/runner_body.sh")"
}

assert_common_success_contract() {
  local remote_os="$1"

  assert_equals "${SCENARIO_STATUS}" "0"
  assert_contains "${SCENARIO_RECORD}" 'REMOTE_FACTS_MARKER=1'
  assert_contains "${SCENARIO_RECORD}" 'ARTIFACT_RUN_MARKER=1'
  assert_contains "${SCENARIO_RECORD}" "REMOTE_OS=${remote_os}"
  assert_contains "${SCENARIO_RECORD}" 'nohup "${runner_path}"'
  assert_not_contains "${SCENARIO_RECORD}" 'launchctl'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'clawbrowser_remote.sh apply-patches'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" 'chromium_remote.sh'
  assert_contains "${SCENARIO_RECORD}" 'build_lock_a="${build_dir_a}.lock"'
  assert_contains "${SCENARIO_RECORD}" 'runner_failed_path="${run_dir}/runner.failed"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'runner_started_path="${run_dir}/runner.started"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'runner_failed_path="${run_dir}/runner.failed"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'icon_assets_dir="${repo_dir}/branding/icons/app/side-bite"'
  assert_contains "${SCENARIO_RECORD}" 'verify_remote_icon_assets'
  assert_contains "${SCENARIO_RECORD}" 'missing required Side Bite icon asset'
  assert_contains "${SCENARIO_RECORD}" 'local icon_assets_dir="${repo_dir}/branding/icons/app/side-bite"'
  assert_contains "${SCENARIO_RECORD}" 'clawbrowser/resources/side_bite.svg'
  assert_contains "${SCENARIO_RECORD}" 'app.icns'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'build_lock_a="${11}"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'release_build_locks'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'refusing to reuse existing run dir'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'else'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'local exit_code=$?'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" $'if "$@"; then\n    append_summary "OK ${name}"\n    printf \'[prod-artifacts] OK %s\\n\' "${name}"\n    return 0\n  fi\n\n  local exit_code=$?'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" 'CLAWBROWSER_API_KEY'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" 'CLAWBROWSER_API_BASE_URL'
  assert_not_contains "${SCENARIO_RECORD}" 'CLAWBROWSER_API_KEY'
  assert_not_contains "${SCENARIO_RECORD}" 'CLAWBROWSER_API_BASE_URL'
  assert_contains "${SCENARIO_RECORD}" 'clawbrowser-prod clawbrowser_default'
  assert_not_contains "${SCENARIO_RECORD}" 'clawbrowser-prod backend_verify'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" '.dmg'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" 'hdiutil'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" 'Applications'
  assert_not_contains "${SCENARIO_OUTPUT}" 'LABEL='

  assert_contains "${SCENARIO_OUTPUT}" "RUN_DIR=${SCENARIO_HOME}/dev/clawbrowser-build-logs/prod-artifacts/prod-artifacts-20260410-200000-CEST-4242"
  assert_contains "${SCENARIO_OUTPUT}" "SUMMARY=${SCENARIO_HOME}/dev/clawbrowser-build-logs/prod-artifacts/prod-artifacts-20260410-200000-CEST-4242/summary.txt"
  assert_contains "${SCENARIO_OUTPUT}" "STATUS=${SCENARIO_HOME}/dev/clawbrowser-build-logs/prod-artifacts/prod-artifacts-20260410-200000-CEST-4242/status.env"
  assert_contains "${SCENARIO_OUTPUT}" "NOHUP=${SCENARIO_HOME}/dev/clawbrowser-build-logs/prod-artifacts/prod-artifacts-20260410-200000-CEST-4242/nohup.log"
  assert_contains "${SCENARIO_OUTPUT}" "META=${SCENARIO_HOME}/dev/clawbrowser-build-logs/prod-artifacts/prod-artifacts-20260410-200000-CEST-4242/launch.env"
  assert_contains "${SCENARIO_OUTPUT}" "LATEST=${SCENARIO_HOME}/dev/clawbrowser-build-logs/prod-artifacts/latest-prod-artifacts"
}

run_darwin_success_scenario() {
  run_launcher Darwin success
  assert_common_success_contract Darwin
  assert_contains "${SCENARIO_OUTPUT}" 'ARTIFACT_COUNT=1'
  assert_contains "${SCENARIO_OUTPUT}" "ARTIFACT_1=${SCENARIO_HOME}/dev/clawbrowser-artifacts/clawbrowser-prod-macos-arm64-20260410-200000-CEST.tar.gz"
  assert_contains "${SCENARIO_RUNNER_BODY}" 'CBProdMacArm64'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'source_app_name="Chromium.app"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'app_name="Clawbrowser.app"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'build_one "${mac_arm64_build_dir}" "arm64"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'local clawbrowser_binary_path="${staged_app}/Contents/MacOS/Clawbrowser"'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" 'local wrapper_path="${staged_app}/Contents/MacOS/Clawbrowser"'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" 'local real_binary_path="${staged_app}/Contents/MacOS/Clawbrowser.real"'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" 'Clawbrowser.real'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'copy_macos_runtime_dylibs "${mac_arm64_build_dir}" "${staged_app}"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'rewrite_macos_bundle_metadata "${staged_app}"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'APP_NAME = "Clawbrowser"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'SOURCE_APP_NAME = "Chromium"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'BUNDLE_VERSION = "1.0.0"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'DIRECT_LAUNCH_SCHEME = "clawbrowser"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'bundle["CFBundleExecutable"] = APP_NAME'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'bundle["CFBundleShortVersionString"] = BUNDLE_VERSION'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'bundle["CFBundleVersion"] = BUNDLE_VERSION'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'bundle["CFBundleIconFile"] = "app"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'def rebrand_nested_helper_bundles(app_path, root_plist_path):'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'nested_bundle["CFBundleName"] = helper_name'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'nested_bundle["CFBundleDisplayName"] = helper_name'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'rebrand_nested_helper_bundles(app_path, plist_path)'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'url_type["CFBundleURLSchemes"] = [DIRECT_LAUNCH_SCHEME]'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" 'bundle["CFBundleIconName"] = "AppIcon"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'env.pop("CLAWBROWSER_DEFAULT_FINGERPRINT_ID", None)'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" 'env["CLAWBROWSER_DEFAULT_FINGERPRINT_ID"] = fingerprint_id'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'overlay_macos_app_icons() {'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'cp "${icon_assets_dir}/app.icns" "${resources_dir}/app.icns"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'rm -f "${resources_dir}/Assets.car"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'overlay_macos_app_icons "${staged_app}"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'sign_macos_app_bundle "${staged_app}"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'codesign --force --deep --sign - "${staged_app}"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'run_step package_macos_arm64 package_macos_arm64'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" 'CBProdMacX64'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" 'build_one "${mac_x64_build_dir}" "x64"'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" 'package_macos_universal'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" 'universalizer.py'
}

run_darwin_icon_sync_scenario() {
  run_launcher Darwin success 0
  assert_common_success_contract Darwin
  assert_contains "${SCENARIO_RSYNC_RECORD}" "${REPO_ROOT}/"
  assert_contains "${SCENARIO_RSYNC_RECORD}" "${REPO_ROOT}/branding/icons/app/side-bite/"
  assert_contains "${SCENARIO_RSYNC_RECORD}" "${REPO_ROOT}/clawbrowser/resources/side_bite.svg"
}

run_linux_success_scenario() {
  run_launcher Linux success
  assert_common_success_contract Linux
  assert_contains "${SCENARIO_RECORD}" 'build_lock_b="${build_dir_b}.lock"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'build_lock_b="${12}"'
  assert_contains "${SCENARIO_OUTPUT}" 'ARTIFACT_COUNT=2'
  assert_contains "${SCENARIO_OUTPUT}" "ARTIFACT_1=${SCENARIO_HOME}/dev/clawbrowser-artifacts/clawbrowser-prod-linux-x64-20260410-200000-CEST.tar.gz"
  assert_contains "${SCENARIO_OUTPUT}" "ARTIFACT_2=${SCENARIO_HOME}/dev/clawbrowser-artifacts/clawbrowser-prod-linux-arm64-20260410-200000-CEST.tar.gz"
  assert_contains "${SCENARIO_RUNNER_BODY}" 'CBProdLinuxX64'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'CBProdLinuxArm64'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'run_step ensure_linux_sysroots ensure_linux_sysroots'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'install-sysroot.py --arch=amd64'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'install-sysroot.py --arch=arm64'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'calculate_linux_parallel_jobs() {'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'linux_parallel_jobs="$(calculate_linux_parallel_jobs)"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'autoninja -C "${build_dir}" -j "${ninja_jobs}" chrome'
  assert_contains "${SCENARIO_RUNNER_BODY}" "setsid bash -c 'set -euo pipefail; build_one \"\$1\" \"\$2\" \"\$3\"'"
  assert_contains "${SCENARIO_RUNNER_BODY}" 'run_parallel_linux_builds() {'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'terminate_linux_build_group() {'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'overlay_linux_bundle_icons() {'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'prune_linux_runtime_noise() {'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'assert_linux_runtime_clean() {'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'local x64_real_binary_path="${bundle_x64}/clawbrowser.real"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'local arm64_real_binary_path="${bundle_arm64}/clawbrowser.real"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'stage_linux_runtime_bundle() {'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'write_linux_wrapper "${bundle_dir}/clawbrowser"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'exec "${SELF_DIR}/clawbrowser.real" --disable-features=DialMediaRouteProvider "$@"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'write_linux_appimage_apprun() {'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'write_linux_appimage_desktop_file() {'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'package_linux_appimage() {'
  assert_contains "${SCENARIO_RUNNER_BODY}" "--include='/vk_swiftshader_icd.json'"
  assert_contains "${SCENARIO_RUNNER_BODY}" "--include='/locales/'"
  assert_contains "${SCENARIO_RUNNER_BODY}" "--include='/locales/*.pak'"
  assert_contains "${SCENARIO_RUNNER_BODY}" "--exclude='/*.json'"
  assert_contains "${SCENARIO_RUNNER_BODY}" "--exclude='/*.TOC'"
  assert_contains "${SCENARIO_RUNNER_BODY}" "--include='/*.so.[0-9]*'"
  assert_contains "${SCENARIO_RUNNER_BODY}" 'prune_linux_runtime_noise "${bundle_dir}"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'assert_linux_runtime_clean "${bundle_dir}"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'if [[ -n "${appimage_release_name}" ]]; then'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'run_step package_linux_appimages package_linux_appimages'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" '--fingerprint='
  assert_not_contains "${SCENARIO_RUNNER_BODY}" '--regenerate'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" '--skip-verify'
  assert_not_contains "${SCENARIO_OUTPUT}" 'ARTIFACT_3='
  assert_not_contains "${SCENARIO_OUTPUT}" 'ARTIFACT_4='
  assert_contains "${SCENARIO_RUNNER_BODY}" 'cp "${icon_assets_dir}/product_logo_256.png" "${bundle_dir}/product_logo_256.png"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'cp "${icon_assets_dir}/product_logo_1024.png" "${bundle_dir}/product_logo_1024.png"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'overlay_linux_bundle_icons "${bundle_dir}"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'kill -- "-${build_group_pid}"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'build_linux_x64_pid="$!"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'build_linux_arm64_pid="$!"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'terminate_linux_build_group "${build_linux_arm64_pid}"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'terminate_linux_build_group "${build_linux_x64_pid}"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'run_step build_linux_parallel run_parallel_linux_builds'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" 'run_step build_linux_x64 build_one "${linux_x64_build_dir}" "x64"'
  assert_not_contains "${SCENARIO_RUNNER_BODY}" 'run_step build_linux_arm64 build_one "${linux_arm64_build_dir}" "arm64"'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'run_step package_linux_archives package_linux_archives'
}

run_linux_appimage_success_scenario() {
  run_launcher Linux success 1 human-release
  assert_contains "${SCENARIO_RECORD}" 'human-release'
  assert_contains "${SCENARIO_OUTPUT}" 'ARTIFACT_COUNT=4'
  assert_contains "${SCENARIO_OUTPUT}" "ARTIFACT_1=${SCENARIO_HOME}/dev/clawbrowser-artifacts/clawbrowser-prod-linux-x64-20260410-200000-CEST.tar.gz"
  assert_contains "${SCENARIO_OUTPUT}" "ARTIFACT_2=${SCENARIO_HOME}/dev/clawbrowser-artifacts/clawbrowser-prod-linux-arm64-20260410-200000-CEST.tar.gz"
  assert_contains "${SCENARIO_OUTPUT}" "ARTIFACT_3=${SCENARIO_HOME}/dev/clawbrowser-artifacts/clawbrowser-human-release-x64-20260410-200000-CEST.AppImage"
  assert_contains "${SCENARIO_OUTPUT}" "ARTIFACT_4=${SCENARIO_HOME}/dev/clawbrowser-artifacts/clawbrowser-human-release-arm64-20260410-200000-CEST.AppImage"
  assert_contains "${SCENARIO_RUNNER_BODY}" 'stage_linux_runtime_bundle() {'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'write_linux_appimage_apprun() {'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'write_linux_appimage_desktop_file() {'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'package_linux_appimage() {'
  assert_contains "${SCENARIO_RUNNER_BODY}" 'run_step package_linux_appimages package_linux_appimages'
}

run_busy_failure_scenario() {
  run_launcher Linux busy
  assert_equals "${SCENARIO_STATUS}" "2"
  assert_contains "${SCENARIO_OUTPUT}" 'BUILD_DIR_BUSY=1'
  assert_contains "${SCENARIO_OUTPUT}" "BUILD_DIR_A=${SCENARIO_HOME}/work/chromium/src/out/CBProdLinuxX64"
  assert_contains "${SCENARIO_OUTPUT}" "BUILD_DIR_B=${SCENARIO_HOME}/work/chromium/src/out/CBProdLinuxArm64"
  assert_contains "${SCENARIO_OUTPUT}" "LOCK_PATH_A=${SCENARIO_HOME}/work/chromium/src/out/CBProdLinuxX64.lock"
  assert_contains "${SCENARIO_OUTPUT}" "LOCK_PATH_B=${SCENARIO_HOME}/work/chromium/src/out/CBProdLinuxArm64.lock"
}

run_start_failure_scenario() {
  run_launcher Darwin start-fail
  assert_equals "${SCENARIO_STATUS}" "1"
  assert_contains "${SCENARIO_OUTPUT}" 'FAILED_TO_START=1'
  assert_contains "${SCENARIO_OUTPUT}" "RUN_DIR=${SCENARIO_HOME}/dev/clawbrowser-build-logs/prod-artifacts/prod-artifacts-20260410-200000-CEST-4242"
  assert_contains "${SCENARIO_OUTPUT}" 'REASON=runner did not create start sentinel'
}

trap cleanup_scenario EXIT

run_darwin_success_scenario
run_darwin_icon_sync_scenario
run_linux_success_scenario
run_linux_appimage_success_scenario
run_busy_failure_scenario
run_start_failure_scenario

printf 'PASS\n'
