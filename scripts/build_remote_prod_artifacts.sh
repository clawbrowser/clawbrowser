#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

REMOTE_HOST="${REMOTE_HOST:-m1@62.210.166.239}"
SSH_KEY="${SSH_KEY:-${HOME}/.ssh/id_ed25519_macmini}"
REMOTE_OS="${REMOTE_OS:-}"
REMOTE_HOME="${REMOTE_HOME:-}"
REMOTE_REPO_DIR="${REMOTE_REPO_DIR:-}"
REMOTE_CHROMIUM_DIR="${REMOTE_CHROMIUM_DIR:-}"
REMOTE_BUILD_ROOT="${REMOTE_BUILD_ROOT:-}"
REMOTE_ARTIFACT_DIR="${REMOTE_ARTIFACT_DIR:-}"
REMOTE_LOG_DIR="${REMOTE_LOG_DIR:-}"
ARTIFACT_BASENAME="${ARTIFACT_BASENAME:-clawbrowser-prod}"
FINGERPRINT_ID="${FINGERPRINT_ID:-clawbrowser_default}"
CLAWBROWSER_BUNDLE_VERSION="${CLAWBROWSER_BUNDLE_VERSION:-1.0.0}"
APPIMAGE_RELEASE_NAME="${APPIMAGE_RELEASE_NAME:-}"
EMPTY_REMOTE_ARG_SENTINEL="__CLAWBROWSER_EMPTY_REMOTE_ARG__"
SKIP_RSYNC=0
SIDE_BITE_ICON_DIR_RELATIVE="branding/icons/app/side-bite"
SIDE_BITE_SVG_RELATIVE="clawbrowser/resources/side_bite.svg"
SIDE_BITE_ICON_ASSETS=(
  app.icns
  product_logo_16.png
  product_logo_22.png
  product_logo_24.png
  product_logo_32.png
  product_logo_48.png
  product_logo_64.png
  product_logo_128.png
  product_logo_256.png
  product_logo_512.png
  product_logo_1024.png
)

log() {
  printf '[remote-prod-artifacts] %s\n' "$*"
}

die() {
  printf '[remote-prod-artifacts] ERROR: %s\n' "$*" >&2
  exit 1
}

usage() {
  cat <<EOF
Usage:
  scripts/build_remote_prod_artifacts.sh [options]

This script:
1. detects the remote host OS over SSH
2. rsyncs the current repo to the remote builder
3. ensures the Side Bite desktop icon assets are present on the remote builder
4. launches one detached remote prod artifact job
5. produces tar.gz artifacts based on the remote OS:
   - macOS: one arm64 macOS tarball
   - Linux: one x64 tarball and one arm64 tarball
6. prints run metadata and expected artifact paths immediately

Required environment:
  None

Options:
  --skip-rsync               Do not sync the repo before launching; remote icon assets are still preflighted
  --remote-host USER@HOST    Default: ${REMOTE_HOST}
  --ssh-key PATH             Default: ${SSH_KEY}
  --remote-repo-dir PATH     Default: remote-home-derived
  --remote-chromium-dir PATH Default: remote-home-derived
  --remote-build-root PATH   Default: <remote chromium>/src/out
  --remote-artifact-dir PATH Default: remote-home-derived
  --remote-log-dir PATH      Default: remote-home-derived
  --artifact-basename NAME   Default: ${ARTIFACT_BASENAME}
  --fingerprint-id ID        Default: ${FINGERPRINT_ID}
  --bundle-version VERSION   macOS bundle CFBundleShortVersionString/CFBundleVersion. Default: ${CLAWBROWSER_BUNDLE_VERSION}
  --appimage-release-name NAME Optional Linux AppImage release label. When set, Linux also emits human-friendly AppImage artifacts such as clawbrowser-human-release-x64.AppImage.
  --help                     Show this help
EOF
}

assert_no_spaces() {
  local path="$1"
  local label="$2"
  if [[ "${path}" == *" "* ]]; then
    die "${label} must not contain spaces: ${path}"
  fi
}

ssh_cmd() {
  ssh -i "${SSH_KEY}" -o IdentitiesOnly=yes "${REMOTE_HOST}" "$@"
}

parse_args() {
  while (($# > 0)); do
    case "$1" in
      --skip-rsync)
        SKIP_RSYNC=1
        shift
        ;;
      --remote-host)
        REMOTE_HOST="$2"
        shift 2
        ;;
      --ssh-key)
        SSH_KEY="$2"
        shift 2
        ;;
      --remote-repo-dir)
        REMOTE_REPO_DIR="$2"
        shift 2
        ;;
      --remote-chromium-dir)
        REMOTE_CHROMIUM_DIR="$2"
        shift 2
        ;;
      --remote-build-root)
        REMOTE_BUILD_ROOT="$2"
        shift 2
        ;;
      --remote-artifact-dir)
        REMOTE_ARTIFACT_DIR="$2"
        shift 2
        ;;
      --remote-log-dir)
        REMOTE_LOG_DIR="$2"
        shift 2
        ;;
      --artifact-basename)
        ARTIFACT_BASENAME="$2"
        shift 2
        ;;
      --fingerprint-id)
        FINGERPRINT_ID="$2"
        shift 2
        ;;
      --bundle-version)
        CLAWBROWSER_BUNDLE_VERSION="$2"
        shift 2
        ;;
      --appimage-release-name)
        APPIMAGE_RELEASE_NAME="$2"
        shift 2
        ;;
      --help|-h)
        usage
        exit 0
        ;;
      *)
        die "Unknown option: $1"
        ;;
    esac
  done
}

build_remote_facts_script() {
  cat <<'EOF'
set -euo pipefail
printf 'REMOTE_OS=%s\n' "$(uname -s)"
printf 'REMOTE_HOME=%s\n' "$HOME"
EOF
}

load_remote_facts() {
  local remote_output
  local remote_status

  set +e
  remote_output="$(
    {
      printf '# REMOTE_FACTS_MARKER=1\n'
      build_remote_facts_script
    } | ssh_cmd "bash -s"
  )"
  remote_status=$?
  set -e

  if ((remote_status != 0)); then
    [[ -n "${remote_output}" ]] && printf '%s\n' "${remote_output}"
    die "Failed to detect remote host facts."
  fi

  REMOTE_OS="$(printf '%s\n' "${remote_output}" | sed -n 's/^REMOTE_OS=//p' | head -n 1)"
  REMOTE_HOME="$(printf '%s\n' "${remote_output}" | sed -n 's/^REMOTE_HOME=//p' | head -n 1)"

  case "${REMOTE_OS}" in
    Darwin|Linux)
      ;;
    *)
      die "Unsupported remote OS: ${REMOTE_OS:-unknown}"
      ;;
  esac

  [[ -n "${REMOTE_HOME}" ]] || die "Failed to detect remote home directory."
}

apply_remote_defaults() {
  if [[ -z "${REMOTE_REPO_DIR}" ]]; then
    REMOTE_REPO_DIR="${REMOTE_HOME}/dev/clawbrowser"
  fi
  if [[ -z "${REMOTE_CHROMIUM_DIR}" ]]; then
    REMOTE_CHROMIUM_DIR="${REMOTE_HOME}/work/chromium"
  fi
  if [[ -z "${REMOTE_BUILD_ROOT}" ]]; then
    REMOTE_BUILD_ROOT="${REMOTE_CHROMIUM_DIR}/src/out"
  fi
  if [[ -z "${REMOTE_ARTIFACT_DIR}" ]]; then
    REMOTE_ARTIFACT_DIR="${REMOTE_HOME}/dev/clawbrowser-artifacts"
  fi
  if [[ -z "${REMOTE_LOG_DIR}" ]]; then
    REMOTE_LOG_DIR="${REMOTE_HOME}/dev/clawbrowser-build-logs/prod-artifacts"
  fi
}

rsync_repo() {
  log "Syncing ${REPO_ROOT} to ${REMOTE_HOST}:${REMOTE_REPO_DIR}"
  rsync -av --delete \
    -e "ssh -i ${SSH_KEY} -o IdentitiesOnly=yes" \
    "${REPO_ROOT}/" \
    "${REMOTE_HOST}:${REMOTE_REPO_DIR}/"
}

assert_local_icon_assets_present() {
  local asset
  local asset_path

  [[ -f "${REPO_ROOT}/${SIDE_BITE_SVG_RELATIVE}" ]] || \
    die "Side Bite SVG missing: ${REPO_ROOT}/${SIDE_BITE_SVG_RELATIVE}"

  for asset in "${SIDE_BITE_ICON_ASSETS[@]}"; do
    asset_path="${REPO_ROOT}/${SIDE_BITE_ICON_DIR_RELATIVE}/${asset}"
    [[ -f "${asset_path}" ]] || die "Side Bite desktop icon asset missing: ${asset_path}"
  done
}

sync_icon_assets() {
  log "Syncing Side Bite icon assets to ${REMOTE_HOST}:${REMOTE_REPO_DIR}"
  rsync -av --delete \
    -e "ssh -i ${SSH_KEY} -o IdentitiesOnly=yes" \
    "${REPO_ROOT}/${SIDE_BITE_ICON_DIR_RELATIVE}/" \
    "${REMOTE_HOST}:${REMOTE_REPO_DIR}/${SIDE_BITE_ICON_DIR_RELATIVE}/"
  rsync -av \
    -e "ssh -i ${SSH_KEY} -o IdentitiesOnly=yes" \
    "${REPO_ROOT}/${SIDE_BITE_SVG_RELATIVE}" \
    "${REMOTE_HOST}:${REMOTE_REPO_DIR}/${SIDE_BITE_SVG_RELATIVE}"
}

build_runner_body() {
  cat <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

run_dir="$1"
timestamp="$2"
remote_os="$3"
repo_dir="$4"
chromium_dir="$5"
build_root="$6"
artifact_dir="$7"
artifact_basename="$8"
fingerprint_id="$9"
clawbrowser_bundle_version="${10}"
appimage_release_name="${11}"
build_lock_a="${12}"
build_lock_b="${13}"

source_app_name="Chromium.app"
app_name="Clawbrowser.app"
summary_path="${run_dir}/summary.txt"
status_path="${run_dir}/status.env"
artifact_manifest_path="${run_dir}/artifacts.txt"
runner_started_path="${run_dir}/runner.started"
runner_failed_path="${run_dir}/runner.failed"
mac_arm64_build_dir="${build_root}/CBProdMacArm64"
linux_x64_build_dir="${build_root}/CBProdLinuxX64"
linux_arm64_build_dir="${build_root}/CBProdLinuxArm64"
icon_assets_dir="${repo_dir}/branding/icons/app/side-bite"
artifact_count=0
artifact_1=""
artifact_2=""
artifact_3=""
artifact_4=""
cleanup_dir_1=""
cleanup_dir_2=""
cleanup_dir_3=""

timestamp_now() {
  date '+%Y-%m-%d %H:%M:%S %Z'
}

append_summary() {
  printf '[%s] %s\n' "$(timestamp_now)" "$*" >>"${summary_path}"
}

write_status() {
  local state="$1"
  local step="$2"
  local exit_code="${3:-0}"

  {
    printf 'state=%s\n' "${state}"
    printf 'step=%s\n' "${step}"
    printf 'exit_code=%s\n' "${exit_code}"
    printf 'last_update=%s\n' "$(timestamp_now)"
    printf 'run_dir=%s\n' "${run_dir}"
    printf 'repo_dir=%s\n' "${repo_dir}"
    printf 'chromium_dir=%s\n' "${chromium_dir}"
    printf 'build_root=%s\n' "${build_root}"
    printf 'artifact_dir=%s\n' "${artifact_dir}"
    printf 'artifact_manifest_path=%s\n' "${artifact_manifest_path}"
    printf 'artifact_count=%s\n' "${artifact_count}"
    printf 'artifact_1=%s\n' "${artifact_1}"
    printf 'artifact_2=%s\n' "${artifact_2}"
    printf 'artifact_3=%s\n' "${artifact_3}"
    printf 'artifact_4=%s\n' "${artifact_4}"
    printf 'summary_path=%s\n' "${summary_path}"
  } >"${status_path}"
}

record_artifacts() {
  : >"${artifact_manifest_path}"
  if [[ -n "${artifact_1}" ]]; then
    printf '%s\n' "${artifact_1}" >>"${artifact_manifest_path}"
  fi
  if [[ -n "${artifact_2}" ]]; then
    printf '%s\n' "${artifact_2}" >>"${artifact_manifest_path}"
  fi
  if [[ -n "${artifact_3}" ]]; then
    printf '%s\n' "${artifact_3}" >>"${artifact_manifest_path}"
  fi
  if [[ -n "${artifact_4}" ]]; then
    printf '%s\n' "${artifact_4}" >>"${artifact_manifest_path}"
  fi
}

cleanup_stage_dirs() {
  [[ -n "${cleanup_dir_1}" ]] && rm -rf "${cleanup_dir_1}"
  [[ -n "${cleanup_dir_2}" ]] && rm -rf "${cleanup_dir_2}"
  [[ -n "${cleanup_dir_3}" ]] && rm -rf "${cleanup_dir_3}"
}

read_lock_owner() {
  local lock_path="$1"
  if [[ -z "${lock_path}" ]]; then
    return 0
  fi
  local owner_path="${lock_path}/owner"
  if [[ -f "${owner_path}" ]]; then
    cat "${owner_path}" 2>/dev/null || true
  fi
}

release_build_lock() {
  local lock_path="$1"
  local owner=""

  if [[ -z "${lock_path}" ]]; then
    return 0
  fi

  owner="$(read_lock_owner "${lock_path}")"
  if [[ -d "${lock_path}" && "${owner}" == "${run_dir}" ]]; then
    rm -rf "${lock_path}"
  fi
}

release_build_locks() {
  release_build_lock "${build_lock_a}"
  release_build_lock "${build_lock_b}"
}

cleanup_on_exit() {
  release_build_locks
  cleanup_stage_dirs
}

trap cleanup_on_exit EXIT

write_startup_failure() {
  local reason="$1"
  {
    printf 'REASON=%s\n' "${reason}"
    printf 'RUN_DIR=%s\n' "${run_dir}"
  } >"${runner_failed_path}"
}

require_owned_lock() {
  local lock_path="$1"
  local build_dir="$2"
  local owner=""

  if [[ -z "${lock_path}" ]]; then
    return 0
  fi

  if [[ ! -d "${lock_path}" ]]; then
    write_startup_failure "missing lock for ${build_dir}"
    printf 'LOCK_PATH=%s\n' "${lock_path}" >>"${runner_failed_path}"
    return 1
  fi

  owner="$(read_lock_owner "${lock_path}")"
  if [[ "${owner}" != "${run_dir}" ]]; then
    write_startup_failure "lock owner mismatch for ${build_dir}"
    {
      printf 'LOCK_PATH=%s\n' "${lock_path}"
      printf 'LOCK_OWNER=%s\n' "${owner}"
    } >>"${runner_failed_path}"
    return 1
  fi
}

mark_runner_started() {
  if ! ( set -o noclobber; : >"${runner_started_path}" ) 2>/dev/null; then
    write_startup_failure "refusing to reuse existing run dir ${run_dir}"
    printf '[prod-artifacts] refusing to reuse existing run dir %s\n' "${run_dir}" >&2
    exit 98
  fi

  : >"${summary_path}"
  append_summary "START prod artifact build"
  write_status running setup 0
  rm -f "${runner_failed_path}"
}

run_step() {
  local name="$1"
  shift

  append_summary "START ${name}"
  printf '[prod-artifacts] START %s\n' "${name}"

  if "$@"; then
    append_summary "OK ${name}"
    printf '[prod-artifacts] OK %s\n' "${name}"
    return 0
  else
    local exit_code=$?
    append_summary "FAIL ${name} exit=${exit_code}"
    printf '[prod-artifacts] FAIL %s exit=%s\n' "${name}" "${exit_code}"
    write_status failed "${name}" "${exit_code}"
    exit "${exit_code}"
  fi
}

prepare_project() {
  cd "${repo_dir}"
  bash scripts/clawbrowser_remote.sh apply-patches \
    --chromium-dir "${chromium_dir}" \
    --project-dir "${repo_dir}"
}

verify_fingerprint_patch_contract() {
  cd "${repo_dir}"
  bash scripts/fingerprint_patch_contract_test.sh
}

verify_staged_runtime() {
  local staged_build_dir="$1"

  cd "${repo_dir}"
  bash scripts/clawbrowser_remote.sh integration-test \
    --chromium-dir "${chromium_dir}" \
    --project-dir "${repo_dir}" \
    --build-dir "${staged_build_dir}"
}

resolve_ccache_bin() {
  if command -v ccache >/dev/null 2>&1; then
    command -v ccache
    return 0
  fi
  if [[ -x "/opt/homebrew/bin/ccache" ]]; then
    printf '%s\n' "/opt/homebrew/bin/ccache"
    return 0
  fi
  if [[ -x "/usr/local/bin/ccache" ]]; then
    printf '%s\n' "/usr/local/bin/ccache"
    return 0
  fi
  if [[ -x "${HOME}/opt/ccache/bin/ccache" ]]; then
    printf '%s\n' "${HOME}/opt/ccache/bin/ccache"
    return 0
  fi
  return 1
}

write_prod_args() {
  local build_dir="$1"
  local target_cpu="$2"
  local args_file="${build_dir}/args.gn"
  local ccache_bin=""

  mkdir -p "${build_dir}"
  ccache_bin="$(resolve_ccache_bin || true)"

  {
    printf '%s\n' 'is_official_build = true'
    printf '%s\n' 'is_component_build = false'
    printf '%s\n' 'chrome_pgo_phase = 0'
    printf '%s\n' 'symbol_level = 0'
    printf '%s\n' 'root_extra_deps = [ "//clawbrowser" ]'
    printf 'target_cpu = "%s"\n' "${target_cpu}"
  } >"${args_file}"

  if [[ -n "${ccache_bin}" ]]; then
    mkdir -p "${HOME}/cache/ccache"
    printf 'cc_wrapper = "env CCACHE_DIR=%s CCACHE_SLOPPINESS=time_macros %s"\n' \
      "${HOME}/cache/ccache" "${ccache_bin}" >>"${args_file}"
  fi
}

require_icon_asset() {
  local asset_name="$1"
  local asset_path="${icon_assets_dir}/${asset_name}"

  if [[ ! -f "${asset_path}" ]]; then
    append_summary "missing icon asset ${asset_path}"
    printf '[prod-artifacts] missing icon asset %s\n' "${asset_path}" >&2
    return 1
  fi

  return 0
}

build_one() {
  local build_dir="$1"
  local target_cpu="$2"
  local ninja_jobs="${3:-}"

  export PATH="${PATH}:${HOME}/opt/depot_tools"
  write_prod_args "${build_dir}" "${target_cpu}"
  cd "${chromium_dir}/src"
  gn gen "${build_dir}"

  if [[ -n "${ninja_jobs}" ]]; then
    autoninja -C "${build_dir}" -j "${ninja_jobs}" chrome
  else
    autoninja -C "${build_dir}" chrome
  fi
}

ensure_linux_sysroots() {
  export PATH="${PATH}:${HOME}/opt/depot_tools"
  cd "${chromium_dir}/src"
  python3 build/linux/sysroot_scripts/install-sysroot.py --arch=amd64
  python3 build/linux/sysroot_scripts/install-sysroot.py --arch=arm64
}

read_linux_cpu_count() {
  local cpu_count=""

  cpu_count="$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)"
  if [[ ! "${cpu_count}" =~ ^[0-9]+$ ]] || (( cpu_count < 1 )); then
    cpu_count="$(nproc 2>/dev/null || true)"
  fi
  if [[ ! "${cpu_count}" =~ ^[0-9]+$ ]] || (( cpu_count < 1 )); then
    cpu_count=1
  fi

  printf '%s\n' "${cpu_count}"
}

read_linux_meminfo_kb() {
  local key="$1"
  local value=""

  if [[ -r "/proc/meminfo" ]]; then
    value="$(awk -v key="${key}" '$1 == key ":" { print $2; exit }' /proc/meminfo 2>/dev/null || true)"
  fi
  if [[ ! "${value}" =~ ^[0-9]+$ ]]; then
    value=0
  fi

  printf '%s\n' "${value}"
}

calculate_linux_parallel_jobs() {
  local cpu_count=1
  local mem_total_kb=0
  local swap_total_kb=0
  local jobs=1

  cpu_count="$(read_linux_cpu_count)"
  mem_total_kb="$(read_linux_meminfo_kb MemTotal)"
  swap_total_kb="$(read_linux_meminfo_kb SwapTotal)"

  jobs=$(( cpu_count / 2 ))
  if (( jobs < 1 )); then
    jobs=1
  fi

  if (( mem_total_kb > 0 && mem_total_kb <= 33554432 && swap_total_kb == 0 && jobs > 2 )); then
    jobs=2
  fi

  printf '%s\n' "${jobs}"
}

launch_linux_build_group() {
  local build_dir="$1"
  local target_cpu="$2"
  local ninja_jobs="$3"

  command -v setsid >/dev/null 2>&1
  export PATH HOME chromium_dir
  export -f resolve_ccache_bin
  export -f write_prod_args
  export -f build_one

  setsid bash -c 'set -euo pipefail; build_one "$1" "$2" "$3"' \
    _ "${build_dir}" "${target_cpu}" "${ninja_jobs}" &
}

terminate_linux_build_group() {
  local build_group_pid="$1"

  if [[ -z "${build_group_pid}" ]]; then
    return 0
  fi

  kill -- "-${build_group_pid}" 2>/dev/null || true
}

run_parallel_linux_builds() {
  local build_linux_x64_pid=""
  local build_linux_arm64_pid=""
  local build_linux_x64_status=0
  local build_linux_arm64_status=0
  local build_linux_x64_done=0
  local build_linux_arm64_done=0
  local first_failure_status=0
  local linux_parallel_jobs=""

  linux_parallel_jobs="$(calculate_linux_parallel_jobs)"
  printf '[prod-artifacts] launching Linux x64 and arm64 builds in parallel\n'
  printf '[prod-artifacts] linux_parallel_jobs=%s\n' "${linux_parallel_jobs}"
  append_summary "START parallel linux builds"
  append_summary "linux_parallel_jobs=${linux_parallel_jobs}"

  launch_linux_build_group "${linux_x64_build_dir}" "x64" "${linux_parallel_jobs}"
  build_linux_x64_pid="$!"

  launch_linux_build_group "${linux_arm64_build_dir}" "arm64" "${linux_parallel_jobs}"
  build_linux_arm64_pid="$!"

  printf '[prod-artifacts] build_linux_x64 pid=%s\n' "${build_linux_x64_pid}"
  printf '[prod-artifacts] build_linux_arm64 pid=%s\n' "${build_linux_arm64_pid}"
  append_summary "build_linux_x64 pid=${build_linux_x64_pid}"
  append_summary "build_linux_arm64 pid=${build_linux_arm64_pid}"

  while (( build_linux_x64_done == 0 || build_linux_arm64_done == 0 )); do
    if (( build_linux_x64_done == 0 )) && ! kill -0 "${build_linux_x64_pid}" 2>/dev/null; then
      if wait "${build_linux_x64_pid}"; then
        build_linux_x64_status=0
      else
        build_linux_x64_status=$?
      fi
      build_linux_x64_done=1
      printf '[prod-artifacts] build_linux_x64 exit=%s\n' "${build_linux_x64_status}"
      append_summary "build_linux_x64 exit=${build_linux_x64_status}"

      if (( build_linux_x64_status != 0 && first_failure_status == 0 )); then
        first_failure_status="${build_linux_x64_status}"
        if (( build_linux_arm64_done == 0 )); then
          printf '[prod-artifacts] terminating build_linux_arm64 pid=%s after x64 failure\n' "${build_linux_arm64_pid}"
          append_summary "terminate build_linux_arm64 pid=${build_linux_arm64_pid} after x64 failure"
          terminate_linux_build_group "${build_linux_arm64_pid}"
        fi
      fi
    fi

    if (( build_linux_arm64_done == 0 )) && ! kill -0 "${build_linux_arm64_pid}" 2>/dev/null; then
      if wait "${build_linux_arm64_pid}"; then
        build_linux_arm64_status=0
      else
        build_linux_arm64_status=$?
      fi
      build_linux_arm64_done=1
      printf '[prod-artifacts] build_linux_arm64 exit=%s\n' "${build_linux_arm64_status}"
      append_summary "build_linux_arm64 exit=${build_linux_arm64_status}"

      if (( build_linux_arm64_status != 0 && first_failure_status == 0 )); then
        first_failure_status="${build_linux_arm64_status}"
        if (( build_linux_x64_done == 0 )); then
          printf '[prod-artifacts] terminating build_linux_x64 pid=%s after arm64 failure\n' "${build_linux_x64_pid}"
          append_summary "terminate build_linux_x64 pid=${build_linux_x64_pid} after arm64 failure"
          terminate_linux_build_group "${build_linux_x64_pid}"
        fi
      fi
    fi

    if (( build_linux_x64_done == 0 || build_linux_arm64_done == 0 )); then
      sleep 1
    fi
  done

  if (( first_failure_status != 0 )); then
    return "${first_failure_status}"
  fi

  append_summary "OK parallel linux builds"
  return 0
}

rewrite_macos_bundle_metadata() {
  local staged_app="$1"
  local plist_path="${staged_app}/Contents/Info.plist"

  [[ -f "${plist_path}" ]] || return 1
  python3 - "${staged_app}" "${clawbrowser_bundle_version}" <<'PY'
from pathlib import Path
import plistlib
import sys

APP_NAME = "Clawbrowser"
SOURCE_APP_NAME = "Chromium"
BUNDLE_ID = "ai.clawbrowser.Clawbrowser"
BUNDLE_VERSION = sys.argv[2]
DIRECT_LAUNCH_NAME = "Direct launch URL"
DIRECT_LAUNCH_SCHEME = "clawbrowser"


def normalize_url_types(bundle):
    url_types = bundle.get("CFBundleURLTypes")
    if not isinstance(url_types, list):
        url_types = []

    matched = False
    normalized = []
    for url_type in url_types:
        if not isinstance(url_type, dict):
            normalized.append(url_type)
            continue

        schemes = url_type.get("CFBundleURLSchemes")
        if isinstance(schemes, list):
            scheme_values = [scheme for scheme in schemes if isinstance(scheme, str)]
        else:
            scheme_values = []

        if (url_type.get("CFBundleURLName") == DIRECT_LAUNCH_NAME or
                "chromium" in scheme_values or
                DIRECT_LAUNCH_SCHEME in scheme_values):
            url_type["CFBundleURLName"] = DIRECT_LAUNCH_NAME
            url_type["CFBundleURLSchemes"] = [DIRECT_LAUNCH_SCHEME]
            matched = True

        normalized.append(url_type)

    if not matched:
        normalized.append({
            "CFBundleURLName": DIRECT_LAUNCH_NAME,
            "CFBundleURLSchemes": [DIRECT_LAUNCH_SCHEME],
        })

    bundle["CFBundleURLTypes"] = normalized


def rebrand_nested_helper_bundles(app_path, root_plist_path):
    for nested_plist_path in sorted(app_path.glob("**/*.app/Contents/Info.plist")):
        if nested_plist_path == root_plist_path:
            continue

        with open(nested_plist_path, "rb") as plist_file:
            nested_bundle = plistlib.load(plist_file)

        helper_name = None
        for key in ("CFBundleDisplayName", "CFBundleName"):
            value = nested_bundle.get(key)
            if isinstance(value, str) and SOURCE_APP_NAME in value:
                helper_name = value.replace(SOURCE_APP_NAME, APP_NAME, 1)
                break

        if helper_name is None:
            continue

        nested_bundle["CFBundleName"] = helper_name
        nested_bundle["CFBundleDisplayName"] = helper_name

        with open(nested_plist_path, "wb") as plist_file:
            plistlib.dump(nested_bundle, plist_file, sort_keys=False)


app_path = Path(sys.argv[1])
plist_path = app_path / "Contents" / "Info.plist"
with open(plist_path, "rb") as plist_file:
    bundle = plistlib.load(plist_file)

bundle["CFBundleExecutable"] = APP_NAME
bundle["CFBundleName"] = APP_NAME
bundle["CFBundleDisplayName"] = APP_NAME
bundle["CFBundleIdentifier"] = BUNDLE_ID
bundle["CFBundleShortVersionString"] = BUNDLE_VERSION
bundle["CFBundleVersion"] = BUNDLE_VERSION
bundle["CFBundleIconFile"] = "app"
bundle.pop("CFBundleIconName", None)
normalize_url_types(bundle)
env = bundle.get("LSEnvironment")
if isinstance(env, dict):
    env.pop("CLAWBROWSER_DEFAULT_FINGERPRINT_ID", None)
    bundle["LSEnvironment"] = env

with open(plist_path, "wb") as plist_file:
    plistlib.dump(bundle, plist_file, sort_keys=False)
rebrand_nested_helper_bundles(app_path, plist_path)
PY
}

write_linux_wrapper() {
  local wrapper_path="$1"

  {
    printf '%s\n' '#!/usr/bin/env bash'
    printf '%s\n' 'set -euo pipefail'
    printf '\n'
    printf '%s\n' 'SELF_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"'
    printf '%s\n' 'exec "${SELF_DIR}/clawbrowser.real" --disable-features=DialMediaRouteProvider "$@"'
  } >"${wrapper_path}"

  chmod +x "${wrapper_path}"
}

overlay_macos_app_icons() {
  local staged_app="$1"
  local resources_dir="${staged_app}/Contents/Resources"

  [[ -d "${resources_dir}" ]] || return 1
  require_icon_asset "app.icns" || return 1
  require_icon_asset "product_logo_32.png" || return 1

  cp "${icon_assets_dir}/app.icns" "${resources_dir}/app.icns"
  cp "${icon_assets_dir}/product_logo_32.png" "${resources_dir}/product_logo_32.png"
  rm -f "${resources_dir}/Assets.car"
}

sign_macos_app_bundle() {
  local staged_app="$1"

  codesign --force --deep --sign - "${staged_app}"
}

overlay_linux_bundle_icons() {
  local bundle_dir="$1"

  require_icon_asset "product_logo_16.png" || return 1
  require_icon_asset "product_logo_22.png" || return 1
  require_icon_asset "product_logo_24.png" || return 1
  require_icon_asset "product_logo_32.png" || return 1
  require_icon_asset "product_logo_48.png" || return 1
  require_icon_asset "product_logo_64.png" || return 1
  require_icon_asset "product_logo_128.png" || return 1
  require_icon_asset "product_logo_256.png" || return 1
  require_icon_asset "product_logo_512.png" || return 1
  require_icon_asset "product_logo_1024.png" || return 1

  cp "${icon_assets_dir}/product_logo_16.png" "${bundle_dir}/product_logo_16.png"
  cp "${icon_assets_dir}/product_logo_22.png" "${bundle_dir}/product_logo_22.png"
  cp "${icon_assets_dir}/product_logo_24.png" "${bundle_dir}/product_logo_24.png"
  cp "${icon_assets_dir}/product_logo_32.png" "${bundle_dir}/product_logo_32.png"
  cp "${icon_assets_dir}/product_logo_48.png" "${bundle_dir}/product_logo_48.png"
  cp "${icon_assets_dir}/product_logo_64.png" "${bundle_dir}/product_logo_64.png"
  cp "${icon_assets_dir}/product_logo_128.png" "${bundle_dir}/product_logo_128.png"
  cp "${icon_assets_dir}/product_logo_256.png" "${bundle_dir}/product_logo_256.png"
  cp "${icon_assets_dir}/product_logo_512.png" "${bundle_dir}/product_logo_512.png"
  cp "${icon_assets_dir}/product_logo_1024.png" "${bundle_dir}/product_logo_1024.png"

  if [[ -d "${bundle_dir}/resources" ]]; then
    cp "${icon_assets_dir}/product_logo_16.png" "${bundle_dir}/resources/product_logo_16.png"
    cp "${icon_assets_dir}/product_logo_22.png" "${bundle_dir}/resources/product_logo_22.png"
    cp "${icon_assets_dir}/product_logo_24.png" "${bundle_dir}/resources/product_logo_24.png"
    cp "${icon_assets_dir}/product_logo_32.png" "${bundle_dir}/resources/product_logo_32.png"
    cp "${icon_assets_dir}/product_logo_48.png" "${bundle_dir}/resources/product_logo_48.png"
    cp "${icon_assets_dir}/product_logo_64.png" "${bundle_dir}/resources/product_logo_64.png"
    cp "${icon_assets_dir}/product_logo_128.png" "${bundle_dir}/resources/product_logo_128.png"
    cp "${icon_assets_dir}/product_logo_256.png" "${bundle_dir}/resources/product_logo_256.png"
    cp "${icon_assets_dir}/product_logo_512.png" "${bundle_dir}/resources/product_logo_512.png"
    cp "${icon_assets_dir}/product_logo_1024.png" "${bundle_dir}/resources/product_logo_1024.png"
  fi
}

copy_macos_runtime_dylibs() {
  local build_dir="$1"
  local staged_app="$2"
  local frameworks_dir="${staged_app}/Contents/Frameworks"
  local libcxx_dylib="${build_dir}/libc++_chrome.dylib"

  [[ -d "${frameworks_dir}" ]] || \
    die "Missing macOS bundle frameworks dir: ${frameworks_dir}"
  [[ -f "${libcxx_dylib}" ]] || \
    die "Missing libc++_chrome.dylib in build dir: ${libcxx_dylib}"

  cp "${libcxx_dylib}" "${frameworks_dir}/libc++_chrome.dylib"
}

package_macos_arm64() {
  local stage_dir="${artifact_dir}/stage-macos-arm64-${timestamp}"
  local staged_app="${stage_dir}/${app_name}"
  local chromium_binary_path="${staged_app}/Contents/MacOS/Chromium"
  local clawbrowser_binary_path="${staged_app}/Contents/MacOS/Clawbrowser"

  cleanup_dir_1="${stage_dir}"
  artifact_1="${artifact_dir}/${artifact_basename}-macos-arm64-${timestamp}.tar.gz"
  artifact_2=""
  artifact_count=1

  rm -rf "${stage_dir}" "${artifact_1}"
  mkdir -p "${stage_dir}"
  rsync -a "${mac_arm64_build_dir}/${source_app_name}/" "${staged_app}/"
  mv "${chromium_binary_path}" "${clawbrowser_binary_path}"
  copy_macos_runtime_dylibs "${mac_arm64_build_dir}" "${staged_app}"
  rewrite_macos_bundle_metadata "${staged_app}"
  overlay_macos_app_icons "${staged_app}"
  sign_macos_app_bundle "${staged_app}"
  verify_staged_runtime "${stage_dir}"

  (
    cd "${stage_dir}"
    tar -czf "${artifact_1}" "${app_name}"
  )
}

copy_linux_runtime() {
  local build_dir="$1"
  local stage_dir="$2"
  local catalog_id

  catalog_id="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["catalog_id"])' "${repo_dir}/clawbrowser/fonts/catalog_build.json")"
  [[ "${catalog_id}" =~ ^[a-zA-Z0-9_-]+$ ]] || die "Invalid active font catalog id"
  [[ -f "${build_dir}/clawbrowser-fonts/${catalog_id}/manifest.json" ]] || \
    die "Missing active font catalog: ${catalog_id}"

  mkdir -p "${stage_dir}"
  rsync -a --prune-empty-dirs \
    --include='/chrome' \
    --include='/chrome_crashpad_handler' \
    --include='/chrome_sandbox' \
    --include='/icudtl.dat' \
    --include='/vk_swiftshader_icd.json' \
    --include='/*.pak' \
    --include='/*.bin' \
    --include='/*.dat' \
    --include='/*.so' \
    --exclude='/*.TOC' \
    --include='/*.so.[0-9]*' \
    --include='/*.png' \
    --include='/*.manifest' \
    --include='/locales/' \
    --include='/locales/*.pak' \
    --include='/resources/***' \
    --include='/swiftshader/***' \
    --include='/clawbrowser-fonts/' \
    --include="/clawbrowser-fonts/${catalog_id}/***" \
    --include='/MEIPreload/***' \
    --exclude='/*.json' \
    --exclude='*' \
    "${build_dir}/" "${stage_dir}/"
}

prune_linux_runtime_noise() {
  local stage_dir="$1"

  find "${stage_dir}" -type f \
    \( -name '*.TOC' -o -name 'siso*.json' -o -name 'package.json' -o -name 'v8_build_config.json' -o -name 'v8_features.json' -o -name '*.pak.info' \) \
    -delete
}

assert_linux_runtime_clean() {
  local stage_dir="$1"
  local noisy_paths=""

  noisy_paths="$(
    find "${stage_dir}" -type f \
      \( -name '*.TOC' -o -name 'siso*.json' -o -name 'package.json' -o -name 'v8_build_config.json' -o -name 'v8_features.json' -o -name '*.pak.info' \) \
      -print
  )"

  if [[ -n "${noisy_paths}" ]]; then
    die "unexpected build noise in linux runtime bundle:\n${noisy_paths}"
  fi
}

stage_linux_runtime_bundle() {
  local build_dir="$1"
  local bundle_dir="$2"

  copy_linux_runtime "${build_dir}" "${bundle_dir}"
  overlay_linux_bundle_icons "${bundle_dir}"
  mv "${bundle_dir}/chrome" "${bundle_dir}/clawbrowser.real"
  write_linux_wrapper "${bundle_dir}/clawbrowser"
  prune_linux_runtime_noise "${bundle_dir}"
  assert_linux_runtime_clean "${bundle_dir}"
}

write_linux_appimage_apprun() {
  local appdir="$1"

  cat >"${appdir}/AppRun" <<'APPIMAGE_APPRUN_EOF'
#!/usr/bin/env bash
set -euo pipefail
SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${SELF_DIR}/clawbrowser" "$@"
APPIMAGE_APPRUN_EOF
  chmod 0755 "${appdir}/AppRun"
}

write_linux_appimage_desktop_file() {
  local appdir="$1"
  local appimage_name="$2"

  cat >"${appdir}/${appimage_name}.desktop" <<APPIMAGE_DESKTOP_EOF
[Desktop Entry]
Type=Application
Name=${appimage_name}
Exec=clawbrowser %U
Icon=product_logo_256
Terminal=false
Categories=Network;WebBrowser;
StartupNotify=true
APPIMAGE_DESKTOP_EOF
}

package_linux_appimage() {
  local build_dir="$1"
  local appdir="$2"
  local artifact_path="$3"
  local arch="$4"
  local appimage_name="$5"
  local appimagetool_bin="${APPIMAGETOOL_BIN:-${HOME}/opt/appimagetool/appimagetool}"

  stage_linux_runtime_bundle "${build_dir}" "${appdir}"
  write_linux_appimage_apprun "${appdir}"
  write_linux_appimage_desktop_file "${appdir}" "${appimage_name}"

  [[ -x "${appimagetool_bin}" ]] || \
    die "Missing appimagetool: ${appimagetool_bin}. Run machine-setup on the Linux builder first."

  ARCH="${arch}" "${appimagetool_bin}" "${appdir}" "${artifact_path}"
}

normalize_appimage_release_name() {
  local release_name="$1"

  case "${release_name}" in
    clawbrowser-*)
      printf '%s\n' "${release_name}"
      ;;
    *)
      printf 'clawbrowser-%s\n' "${release_name}"
      ;;
  esac
}

package_linux_archives() {
  local package_root_x64="${artifact_dir}/stage-linux-x64-${timestamp}"
  local package_root_arm64="${artifact_dir}/stage-linux-arm64-${timestamp}"
  local bundle_x64="${package_root_x64}/${artifact_basename}-linux-x64"
  local bundle_arm64="${package_root_arm64}/${artifact_basename}-linux-arm64"
  local x64_browser_path="${bundle_x64}/chrome"
  local arm64_browser_path="${bundle_arm64}/chrome"
  local x64_real_binary_path="${bundle_x64}/clawbrowser.real"
  local arm64_real_binary_path="${bundle_arm64}/clawbrowser.real"

  cleanup_dir_1="${package_root_x64}"
  cleanup_dir_2="${package_root_arm64}"
  artifact_1="${artifact_dir}/${artifact_basename}-linux-x64-${timestamp}.tar.gz"
  artifact_2="${artifact_dir}/${artifact_basename}-linux-arm64-${timestamp}.tar.gz"
  artifact_count=2

  rm -rf "${package_root_x64}" "${package_root_arm64}" "${artifact_1}" "${artifact_2}"
  mkdir -p "${bundle_x64}" "${bundle_arm64}"

  stage_linux_runtime_bundle "${linux_x64_build_dir}" "${bundle_x64}"
  stage_linux_runtime_bundle "${linux_arm64_build_dir}" "${bundle_arm64}"
  # The x64 archive is runnable on the Linux builder, so exercise the exact
  # staged release wrapper and payload before either architecture is archived.
  # Both architectures are compiled from the same patched Chromium sources;
  # the static contract above additionally guards the shared patch set.
  verify_staged_runtime "${bundle_x64}"

  (
    cd "${package_root_x64}"
    tar -czf "${artifact_1}" "$(basename "${bundle_x64}")"
  )
  (
    cd "${package_root_arm64}"
    tar -czf "${artifact_2}" "$(basename "${bundle_arm64}")"
  )
}

package_linux_appimages() {
  local package_root="${artifact_dir}/stage-linux-appimage-${timestamp}"
  local appimage_artifact_basename
  local appdir_x64
  local appdir_arm64

  cleanup_dir_3="${package_root}"
  appimage_artifact_basename="$(normalize_appimage_release_name "${appimage_release_name}")"
  appdir_x64="${package_root}/${appimage_artifact_basename}-x64.AppDir"
  appdir_arm64="${package_root}/${appimage_artifact_basename}-arm64.AppDir"

  artifact_3="${artifact_dir}/${appimage_artifact_basename}-x64.AppImage"
  artifact_4="${artifact_dir}/${appimage_artifact_basename}-arm64.AppImage"
  artifact_count=4

  rm -rf "${package_root}" "${artifact_3}" "${artifact_4}"
  mkdir -p "${appdir_x64}" "${appdir_arm64}"

  package_linux_appimage "${linux_x64_build_dir}" "${appdir_x64}" "${artifact_3}" "x86_64" "${appimage_artifact_basename}"
  package_linux_appimage "${linux_arm64_build_dir}" "${appdir_arm64}" "${artifact_4}" "aarch64" "${appimage_artifact_basename}"
}

case "${remote_os}" in
  Darwin)
    require_owned_lock "${build_lock_a}" "${mac_arm64_build_dir}"
    ;;
  Linux)
    require_owned_lock "${build_lock_a}" "${linux_x64_build_dir}"
    require_owned_lock "${build_lock_b}" "${linux_arm64_build_dir}"
    ;;
  *)
    write_startup_failure "unsupported remote_os in runner: ${remote_os}"
    printf 'unsupported remote_os in runner: %s\n' "${remote_os}" >&2
    exit 1
    ;;
esac

mark_runner_started
run_step fingerprint_contract verify_fingerprint_patch_contract
run_step prepare_project prepare_project

case "${remote_os}" in
  Darwin)
    run_step build_mac_arm64 build_one "${mac_arm64_build_dir}" "arm64"
    run_step package_macos_arm64 package_macos_arm64
    ;;
  Linux)
    run_step ensure_linux_sysroots ensure_linux_sysroots
    run_step build_linux_parallel run_parallel_linux_builds
    run_step package_linux_archives package_linux_archives
    if [[ -n "${appimage_release_name}" ]]; then
      run_step package_linux_appimages package_linux_appimages
    fi
    ;;
esac

record_artifacts
write_status succeeded finished 0
append_summary "SUCCESS artifact_count=${artifact_count}"
printf '[prod-artifacts] SUCCESS count=%s\n' "${artifact_count}"
printf 'ARTIFACT_COUNT=%s\n' "${artifact_count}"
printf 'ARTIFACT_1=%s\n' "${artifact_1}"
if [[ -n "${artifact_2}" ]]; then
  printf 'ARTIFACT_2=%s\n' "${artifact_2}"
fi
if [[ -n "${artifact_3}" ]]; then
  printf 'ARTIFACT_3=%s\n' "${artifact_3}"
fi
if [[ -n "${artifact_4}" ]]; then
  printf 'ARTIFACT_4=%s\n' "${artifact_4}"
fi
EOF
}

build_remote_launcher_body() {
  cat <<'EOF'
set -euo pipefail

decode_optional_arg() {
  case "$1" in
    __CLAWBROWSER_EMPTY_REMOTE_ARG__)
      printf ''
      ;;
    *)
      printf '%s\n' "$1"
      ;;
  esac
}

remote_os="$1"
repo_dir="$2"
chromium_dir="$3"
build_root="$4"
artifact_dir="$5"
log_root="$6"
artifact_basename="$7"
fingerprint_id="$(decode_optional_arg "$8")"
clawbrowser_bundle_version="$(decode_optional_arg "$9")"
appimage_release_name="$(decode_optional_arg "${10}")"
runner_b64="${11:?missing runner payload}"

timestamp="$(date '+%Y%m%d-%H%M%S-%Z')"
run_id="${timestamp}-$$"
run_dir="${log_root}/prod-artifacts-${run_id}"
runner_path="${run_dir}/run.sh"
meta_path="${run_dir}/launch.env"
status_path="${run_dir}/status.env"
summary_path="${run_dir}/summary.txt"
nohup_path="${run_dir}/nohup.log"
runner_started_path="${run_dir}/runner.started"
runner_failed_path="${run_dir}/runner.failed"
latest_link="${log_root}/latest-prod-artifacts"
launch_ok=0
build_lock_a=""
build_lock_b=""
appimage_artifact_basename=""
artifact_3=""
artifact_4=""

normalize_appimage_release_name() {
  local release_name="$1"

  case "${release_name}" in
    clawbrowser-*)
      printf '%s\n' "${release_name}"
      ;;
    *)
      printf 'clawbrowser-%s\n' "${release_name}"
      ;;
  esac
}

read_lock_owner() {
  local lock_path="$1"
  local owner_path="${lock_path}/owner"
  if [[ -f "${owner_path}" ]]; then
    cat "${owner_path}" 2>/dev/null || true
  fi
}

claim_build_lock() {
  local lock_path="$1"
  local owner="$2"
  if mkdir "${lock_path}" 2>/dev/null; then
    printf '%s\n' "${owner}" >"${lock_path}/owner"
    return 0
  fi
  return 1
}

release_build_lock() {
  local lock_path="$1"
  local owner="$2"
  local current_owner=""

  current_owner="$(read_lock_owner "${lock_path}")"
  if [[ -d "${lock_path}" && "${current_owner}" == "${owner}" ]]; then
    rm -rf "${lock_path}"
  fi
}

release_build_locks() {
  release_build_lock "${build_lock_a}" "${run_dir}"
  release_build_lock "${build_lock_b}" "${run_dir}"
}

print_busy_report() {
  printf 'BUILD_DIR_BUSY=1\n'
  printf 'BUILD_DIR_A=%s\n' "${build_dir_a}"
  printf 'BUILD_DIR_B=%s\n' "${build_dir_b}"
  printf 'LOCK_PATH_A=%s\n' "${build_lock_a}"
  printf 'LOCK_PATH_B=%s\n' "${build_lock_b}"
  local owner_a owner_b
  owner_a="$(read_lock_owner "${build_lock_a}")"
  owner_b="$(read_lock_owner "${build_lock_b}")"
  if [[ -n "${owner_a}" ]]; then
    printf 'LOCK_OWNER_A=%s\n' "${owner_a}"
  fi
  if [[ -n "${owner_b}" ]]; then
    printf 'LOCK_OWNER_B=%s\n' "${owner_b}"
  fi
}

cleanup_on_exit() {
  if ((launch_ok == 0)); then
    release_build_locks
  fi
}

trap cleanup_on_exit EXIT

case "${remote_os}" in
  Darwin)
    build_dir_a="${build_root}/CBProdMacArm64"
    build_dir_b=""
    artifact_1="${artifact_dir}/${artifact_basename}-macos-arm64-${timestamp}.tar.gz"
    artifact_2=""
    ;;
  Linux)
    build_dir_a="${build_root}/CBProdLinuxX64"
    build_dir_b="${build_root}/CBProdLinuxArm64"
    artifact_1="${artifact_dir}/${artifact_basename}-linux-x64-${timestamp}.tar.gz"
    artifact_2="${artifact_dir}/${artifact_basename}-linux-arm64-${timestamp}.tar.gz"
    ;;
  *)
    echo "FAILED_TO_START=1"
    echo "REASON=unsupported remote_os ${remote_os}"
    exit 1
    ;;
esac

build_lock_a="${build_dir_a}.lock"
if [[ -n "${build_dir_b}" ]]; then
  build_lock_b="${build_dir_b}.lock"
else
  build_lock_b=""
fi

if [[ -n "${appimage_release_name}" ]]; then
  appimage_artifact_basename="$(normalize_appimage_release_name "${appimage_release_name}")"
  artifact_3="${artifact_dir}/${appimage_artifact_basename}-x64-${timestamp}.AppImage"
  artifact_4="${artifact_dir}/${appimage_artifact_basename}-arm64-${timestamp}.AppImage"
fi

verify_remote_icon_assets() {
  local icon_assets_dir="${repo_dir}/branding/icons/app/side-bite"
  local side_bite_svg="${repo_dir}/clawbrowser/resources/side_bite.svg"
  local asset
  local asset_path

  if [[ ! -f "${side_bite_svg}" ]]; then
    printf 'FAILED_TO_START=1\n'
    printf 'REASON=missing required Side Bite icon asset\n'
    printf 'ICON_ASSET=%s\n' "${side_bite_svg}"
    exit 1
  fi

  for asset in \
    app.icns \
    product_logo_16.png \
    product_logo_22.png \
    product_logo_24.png \
    product_logo_32.png \
    product_logo_48.png \
    product_logo_64.png \
    product_logo_128.png \
    product_logo_256.png \
    product_logo_512.png \
    product_logo_1024.png; do
    asset_path="${icon_assets_dir}/${asset}"
    if [[ ! -f "${asset_path}" ]]; then
      printf 'FAILED_TO_START=1\n'
      printf 'REASON=missing required Side Bite icon asset\n'
      printf 'ICON_ASSET=%s\n' "${asset_path}"
      exit 1
    fi
  done
}

verify_remote_icon_assets

mkdir -p "${artifact_dir}" "${log_root}"

if ! claim_build_lock "${build_lock_a}" "${run_dir}"; then
  print_busy_report
  exit 2
fi

if [[ -n "${build_lock_b}" ]]; then
  if ! claim_build_lock "${build_lock_b}" "${run_dir}"; then
    print_busy_report
    exit 2
  fi
fi

mkdir "${run_dir}"

python3 -c 'import base64, pathlib, sys; pathlib.Path(sys.argv[2]).write_bytes(base64.b64decode(sys.argv[1]))' \
  "${runner_b64}" \
  "${runner_path}"
chmod +x "${runner_path}"

{
  printf 'timestamp=%s\n' "${timestamp}"
  printf 'remote_os=%s\n' "${remote_os}"
  printf 'remote_host=%s\n' "$(hostname)"
  printf 'repo_dir=%s\n' "${repo_dir}"
  printf 'chromium_dir=%s\n' "${chromium_dir}"
  printf 'build_root=%s\n' "${build_root}"
  printf 'artifact_dir=%s\n' "${artifact_dir}"
  printf 'log_root=%s\n' "${log_root}"
  printf 'artifact_basename=%s\n' "${artifact_basename}"
  printf 'fingerprint_id=%s\n' "${fingerprint_id}"
  printf 'clawbrowser_bundle_version=%s\n' "${clawbrowser_bundle_version}"
  printf 'appimage_release_name=%s\n' "${appimage_release_name}"
  printf 'run_dir=%s\n' "${run_dir}"
  printf 'runner_path=%s\n' "${runner_path}"
  printf 'status_path=%s\n' "${status_path}"
  printf 'summary_path=%s\n' "${summary_path}"
  printf 'nohup_path=%s\n' "${nohup_path}"
  printf 'runner_started_path=%s\n' "${runner_started_path}"
  printf 'runner_failed_path=%s\n' "${runner_failed_path}"
  printf 'build_lock_a=%s\n' "${build_lock_a}"
  printf 'build_lock_b=%s\n' "${build_lock_b}"
  printf 'launcher_kind=nohup\n'
} >"${meta_path}"

nohup "${runner_path}" \
  "${run_dir}" \
  "${timestamp}" \
  "${remote_os}" \
  "${repo_dir}" \
  "${chromium_dir}" \
  "${build_root}" \
  "${artifact_dir}" \
  "${artifact_basename}" \
  "${fingerprint_id}" \
  "${clawbrowser_bundle_version}" \
  "${appimage_release_name}" \
  "${build_lock_a}" \
  "${build_lock_b}" >>"${nohup_path}" 2>&1 </dev/null &
pid="$!"

for attempt in 1 2 3 4 5; do
  if [[ -f "${runner_started_path}" ]]; then
    break
  fi
  if [[ -f "${runner_failed_path}" ]]; then
    break
  fi
  if ! kill -0 "${pid}" 2>/dev/null; then
    break
  fi
  sleep 1
done

if [[ ! -f "${runner_started_path}" ]]; then
  echo "FAILED_TO_START=1"
  echo "RUN_DIR=${run_dir}"
  if [[ -f "${runner_failed_path}" ]]; then
    cat "${runner_failed_path}"
  else
    echo "REASON=runner did not create start sentinel"
  fi
  exit 1
fi

launch_ok=1
ln -sfn "${run_dir}" "${latest_link}"

printf 'RUN_DIR=%s\n' "${run_dir}"
printf 'PID=%s\n' "${pid}"
printf 'SUMMARY=%s\n' "${summary_path}"
printf 'STATUS=%s\n' "${status_path}"
printf 'NOHUP=%s\n' "${nohup_path}"
printf 'META=%s\n' "${meta_path}"
if [[ -n "${artifact_4}" ]]; then
  printf 'ARTIFACT_COUNT=4\n'
elif [[ -n "${artifact_2}" ]]; then
  printf 'ARTIFACT_COUNT=2\n'
else
  printf 'ARTIFACT_COUNT=1\n'
fi
printf 'ARTIFACT_1=%s\n' "${artifact_1}"
if [[ -n "${artifact_2}" ]]; then
  printf 'ARTIFACT_2=%s\n' "${artifact_2}"
fi
if [[ -n "${artifact_3}" ]]; then
  printf 'ARTIFACT_3=%s\n' "${artifact_3}"
fi
if [[ -n "${artifact_4}" ]]; then
  printf 'ARTIFACT_4=%s\n' "${artifact_4}"
fi
printf 'LATEST=%s\n' "${latest_link}"
EOF
}

remote_optional_arg() {
  if [[ -n "$1" ]]; then
    printf '%s\n' "$1"
  else
    printf '%s\n' "${EMPTY_REMOTE_ARG_SENTINEL}"
  fi
}

launch_remote_artifacts() {
  local remote_output
  local remote_status
  local runner_b64

  runner_b64="$(build_runner_body | base64 | tr -d '\n')"

  set +e
  remote_output="$(
    {
      printf '# ARTIFACT_RUN_MARKER=1\n'
      printf '# REMOTE_OS=%s\n' "${REMOTE_OS}"
      build_remote_launcher_body
    } | ssh_cmd "bash -s" -- \
      "${REMOTE_OS}" \
      "${REMOTE_REPO_DIR}" \
      "${REMOTE_CHROMIUM_DIR}" \
      "${REMOTE_BUILD_ROOT}" \
      "${REMOTE_ARTIFACT_DIR}" \
      "${REMOTE_LOG_DIR}" \
      "${ARTIFACT_BASENAME}" \
      "$(remote_optional_arg "${FINGERPRINT_ID}")" \
      "$(remote_optional_arg "${CLAWBROWSER_BUNDLE_VERSION}")" \
      "$(remote_optional_arg "${APPIMAGE_RELEASE_NAME}")" \
      "${runner_b64}"
  )"
  remote_status=$?
  set -e

  [[ -n "${remote_output}" ]] && printf '%s\n' "${remote_output}"
  return "${remote_status}"
}

main() {
  parse_args "$@"

  [[ -f "${SSH_KEY}" ]] || die "SSH key not found: ${SSH_KEY}"
  load_remote_facts
  apply_remote_defaults

  assert_no_spaces "${REMOTE_REPO_DIR}" "Remote repo path"
  assert_no_spaces "${REMOTE_CHROMIUM_DIR}" "Remote chromium path"
  assert_no_spaces "${REMOTE_BUILD_ROOT}" "Remote build root"
  assert_no_spaces "${REMOTE_ARTIFACT_DIR}" "Remote artifact path"
  assert_no_spaces "${REMOTE_LOG_DIR}" "Remote log path"
  assert_no_spaces "${ARTIFACT_BASENAME}" "Artifact basename"
  assert_no_spaces "${FINGERPRINT_ID}" "Fingerprint ID"
  assert_no_spaces "${CLAWBROWSER_BUNDLE_VERSION}" "Clawbrowser bundle version"
  if [[ -n "${APPIMAGE_RELEASE_NAME}" ]]; then
    assert_no_spaces "${APPIMAGE_RELEASE_NAME}" "AppImage release name"
  fi
  assert_local_icon_assets_present

  if ((SKIP_RSYNC == 0)); then
    rsync_repo
    sync_icon_assets
  else
    log "Skipping rsync"
  fi

  log "Launching detached prod artifact build on ${REMOTE_HOST} (${REMOTE_OS})"
  launch_remote_artifacts
}

main "$@"
