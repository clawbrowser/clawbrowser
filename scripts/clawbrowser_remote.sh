#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

DEFAULT_PROJECT_DIR="${REPO_ROOT}"
DEFAULT_CHROMIUM_PIN_FILE="${SCRIPT_DIR}/clawbrowser_pin.conf"

DEPOT_TOOLS_DIR="${DEPOT_TOOLS_DIR:-${HOME}/opt/depot_tools}"
CHROMIUM_DIR="${CHROMIUM_DIR:-${HOME}/work/chromium}"
CCACHE_DIR="${CCACHE_DIR:-${HOME}/cache/ccache}"
CCACHE_MAX_SIZE="${CCACHE_MAX_SIZE:-50G}"
CCACHE_BIN="${CCACHE_BIN:-}"
APPIMAGETOOL_DIR="${APPIMAGETOOL_DIR:-${HOME}/opt/appimagetool}"
APPIMAGETOOL_BIN="${APPIMAGETOOL_BIN:-${APPIMAGETOOL_DIR}/appimagetool}"
APPIMAGETOOL_BASE_URL="${APPIMAGETOOL_BASE_URL:-https://github.com/AppImage/AppImageKit/releases/download/continuous}"
BUILD_DIR="${BUILD_DIR:-${CHROMIUM_DIR}/src/out/CBFast}"
TARGET="${TARGET:-chrome}"
INTEGRATION_VENV_DIR="${INTEGRATION_VENV_DIR:-${HOME}/.cache/clawbrowser/integration-venv}"
FINGERPRINT_ID="${FINGERPRINT_ID:-clawbrowser_default}"

PROJECT_DIR="${PROJECT_DIR:-${DEFAULT_PROJECT_DIR}}"
PROJECT_REPO_URL="${PROJECT_REPO_URL:-}"
PROJECT_BRANCH="${PROJECT_BRANCH:-}"
PROJECT_REMOTE="${PROJECT_REMOTE:-origin}"

CHROMIUM_PIN_FILE="${CHROMIUM_PIN_FILE:-${DEFAULT_CHROMIUM_PIN_FILE}}"
CHROMIUM_REVISION="${CHROMIUM_REVISION:-}"
CHROMIUM_VERSION_LABEL="${CHROMIUM_VERSION_LABEL:-}"
CHROMIUM_BRANCH="${CHROMIUM_BRANCH:-}"
CHROMIUM_REMOTE="${CHROMIUM_REMOTE:-origin}"

FETCH_NO_HISTORY=1

log() {
  printf '[clawbrowser-remote] %s\n' "$*"
}

die() {
  printf '[clawbrowser-remote] ERROR: %s\n' "$*" >&2
  exit 1
}

usage() {
  cat <<'EOF'
Usage:
  scripts/clawbrowser_remote.sh <command> [options]

  Commands:
  machine-setup     Check host prerequisites; on macOS install Xcode/MetalToolchain, on Linux install librsvg2-bin and appimagetool, plus depot_tools/ccache
  checkout          First-time Chromium checkout using fetch, then sync to the pinned revision
  update            Resync an existing Chromium checkout to the pinned revision
  sync-project      Sync this project's build-relevant files into chromium/src
  apply-patches     Reset patch-owned files to the pinned revision, sync project files, and apply repo-pinned Chromium patch files
  gen               Write args.gn and run gn gen
  build             Sync project, write args.gn, run gn gen, and build a target
  integration-setup Create/update the Python venv used by Playwright/pytest integration tests
  integration-test  Sync project files and run the Playwright/pytest integration suite against the built browser
  ccache-stats      Show ccache statistics
  ccache-size       Set ccache max size
  tune-git          Apply safe git performance settings to the Chromium checkout
  help              Show this help

Options:
  --depot-tools-dir PATH   Default: ~/opt/depot_tools
  --chromium-dir PATH      Default: ~/work/chromium
  --build-dir PATH         Default: ~/work/chromium/src/out/CBFast
  --project-dir PATH       Default: current repository root
  --project-repo-url URL   Optional git URL for a project repo clone on the remote machine
  --project-branch NAME    Optional project repo branch to checkout
  --project-remote NAME    Default: origin
  --chromium-pin-file PATH Default: scripts/clawbrowser_pin.conf
  --chromium-revision SHA  Override the repo-pinned Chromium revision for this run
  --chromium-branch NAME   Legacy branch checkout flow; incompatible with pinned revision mode
  --chromium-remote NAME   Default: origin
  --target NAME            Default: chrome
  --integration-venv-dir PATH Default: ~/.cache/clawbrowser/integration-venv
  --ccache-dir PATH        Default: ~/cache/ccache
  --ccache-max-size SIZE   Default: 50G
  --with-history           Use fetch chromium instead of fetch --no-history chromium

Examples:
  bash scripts/clawbrowser_remote.sh machine-setup
  bash scripts/clawbrowser_remote.sh checkout
  bash scripts/clawbrowser_remote.sh checkout --chromium-revision 25a94a5662bc9cce918f4626472e7edca1ba2b39
  bash scripts/clawbrowser_remote.sh apply-patches
  bash scripts/clawbrowser_remote.sh build --target chrome
  bash scripts/clawbrowser_remote.sh integration-setup
  bash scripts/clawbrowser_remote.sh integration-test
  bash scripts/clawbrowser_remote.sh build --target //clawbrowser:clawbrowser_unittests
  bash scripts/clawbrowser_remote.sh build --project-repo-url git@github.com:you/clawbrowser.git --project-branch browser-component-impl
EOF
}

command_exists() {
  command -v "$1" >/dev/null 2>&1
}

prepend_common_mac_paths() {
  if [[ -d "/opt/homebrew/bin" ]]; then
    PATH="/opt/homebrew/bin:${PATH}"
  fi
  if [[ -d "/usr/local/bin" ]]; then
    PATH="/usr/local/bin:${PATH}"
  fi
  export PATH
}

prepend_depot_tools_path() {
  PATH="${PATH}:${DEPOT_TOOLS_DIR}"
  export PATH
}

assert_no_spaces() {
  local path="$1"
  local label="$2"
  if [[ "${path}" == *" "* ]]; then
    die "${label} must not contain spaces: ${path}"
  fi
}

require_command() {
  local name="$1"
  local hint="$2"
  if ! command_exists "${name}"; then
    die "Required command not found: ${name}. ${hint}"
  fi
}

require_git_repo() {
  local repo="$1"
  git -C "${repo}" rev-parse --is-inside-work-tree >/dev/null 2>&1 || \
    die "Not a git repository: ${repo}"
}

require_clean_repo() {
  local repo="$1"
  if [[ -n "$(git -C "${repo}" status --porcelain)" ]]; then
    die "Git repository has local changes, refusing to switch/update: ${repo}"
  fi
}

version_ge() {
  local a="$1"
  local b="$2"
  local IFS=.
  local i
  local -a va vb
  read -r -a va <<<"${a}"
  read -r -a vb <<<"${b}"

  for ((i = ${#va[@]}; i < 3; i++)); do
    va[i]=0
  done
  for ((i = ${#vb[@]}; i < 3; i++)); do
    vb[i]=0
  done

  for i in 0 1 2; do
    if ((10#${va[i]} > 10#${vb[i]})); then
      return 0
    fi
    if ((10#${va[i]} < 10#${vb[i]})); then
      return 1
    fi
  done
  return 0
}

ensure_full_xcode() {
  require_command xcode-select "Install full Xcode from the App Store or developer.apple.com."
  require_command xcodebuild "Install full Xcode from the App Store or developer.apple.com."
  require_command xcrun "Install full Xcode from the App Store or developer.apple.com."

  local dev_dir
  dev_dir="$(xcode-select -p)"
  xcodebuild -version >/dev/null 2>&1 || \
    die "xcodebuild failed. Make sure full Xcode is installed and the license is accepted."

  if [[ ! -d "${dev_dir}/Platforms/MacOSX.platform/Developer/SDKs" ]]; then
    die "macOS SDKs not found under ${dev_dir}. Run sudo xcode-select -s /Applications/Xcode.app/Contents/Developer."
  fi

  ensure_xcode_metal_toolchain
}

xcode_metal_toolchain_works() {
  (
    local tmp_dir
    tmp_dir="$(mktemp -d)"
    trap 'rm -rf "${tmp_dir}"' EXIT

    cat >"${tmp_dir}/probe.metal" <<'EOF'
#include <metal_stdlib>
using namespace metal;
kernel void clawbrowser_metal_toolchain_probe() {}
EOF

    xcrun --find metal >/dev/null 2>&1 &&
      xcrun metal -c "${tmp_dir}/probe.metal" \
        -o "${tmp_dir}/probe.air" \
        --std=macos-metal2.1 \
        -mmacosx-version-min=10.14 >/dev/null 2>&1
  )
}

ensure_xcode_metal_toolchain() {
  if xcode_metal_toolchain_works; then
    return 0
  fi

  log "Installing Xcode MetalToolchain component"
  xcodebuild -downloadComponent MetalToolchain

  xcode_metal_toolchain_works || \
    die "Metal compiler still failed after installing MetalToolchain."
}

ensure_base_prereqs() {
  prepend_common_mac_paths
  require_command git "Install Command Line Tools or Xcode."
  require_command python3 "Chromium tooling requires Python 3."
  require_command rsync "rsync is required to sync project files into chromium/src."
  require_command bash "A bash-compatible shell is required."
  ensure_full_xcode
}

ensure_patch_prereqs() {
  prepend_common_mac_paths
  require_command git "Install git first."
  require_command python3 "Chromium tooling requires Python 3."
  require_command rsync "rsync is required to sync project files into chromium/src."
  require_command bash "A bash-compatible shell is required."

  case "$(uname -s)" in
    Linux)
      if ! command_exists rsvg-convert; then
        log "Installing librsvg2-bin for Side Bite asset rendering"
        sudo apt-get update
        sudo apt-get install -y librsvg2-bin
      fi
      ensure_appimagetool
      ;;
  esac
}

normalize_linux_machine_arch() {
  case "$(uname -m)" in
    x86_64|amd64)
      printf '%s\n' 'x86_64'
      ;;
    aarch64|arm64)
      printf '%s\n' 'aarch64'
      ;;
    *)
      die "Unsupported Linux machine architecture for appimagetool: $(uname -m)"
      ;;
  esac
}

download_appimagetool_appimage() {
  local destination="$1"
  local arch="$2"
  local url="${APPIMAGETOOL_BASE_URL}/appimagetool-${arch}.AppImage"

  log "Downloading appimagetool ${arch} to ${destination}"
  mkdir -p "$(dirname "${destination}")"
  python3 - "${url}" "${destination}" <<'PY'
import pathlib
import sys
import urllib.request

url = sys.argv[1]
destination = sys.argv[2]

with urllib.request.urlopen(url) as response:
    pathlib.Path(destination).write_bytes(response.read())
PY
  chmod 0755 "${destination}"
}

write_appimagetool_wrapper() {
  local wrapper_path="$1"
  local appimage_path="$2"

  cat >"${wrapper_path}" <<EOF
#!/usr/bin/env bash
set -euo pipefail
exec "${appimage_path}" --appimage-extract-and-run "\$@"
EOF
  chmod 0755 "${wrapper_path}"
}

ensure_appimagetool() {
  local host_arch
  local appimage_path

  host_arch="$(normalize_linux_machine_arch)"
  mkdir -p "${APPIMAGETOOL_DIR}"

  if command_exists appimagetool; then
    write_appimagetool_wrapper "${APPIMAGETOOL_BIN}" "$(command -v appimagetool)"
    return 0
  fi

  appimage_path="${APPIMAGETOOL_DIR}/appimagetool-${host_arch}.AppImage"
  if [[ ! -x "${appimage_path}" ]]; then
    download_appimagetool_appimage "${appimage_path}" "${host_arch}"
  fi
  write_appimagetool_wrapper "${APPIMAGETOOL_BIN}" "${appimage_path}"
  require_command "${APPIMAGETOOL_BIN}" \
    "appimagetool wrapper missing after installation."
}

ensure_depot_tools() {
  if [[ ! -d "${DEPOT_TOOLS_DIR}/.git" ]]; then
    log "Cloning depot_tools into ${DEPOT_TOOLS_DIR}"
    mkdir -p "$(dirname "${DEPOT_TOOLS_DIR}")"
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "${DEPOT_TOOLS_DIR}"
  else
    log "Updating depot_tools in ${DEPOT_TOOLS_DIR}"
    require_clean_repo "${DEPOT_TOOLS_DIR}"
    git -C "${DEPOT_TOOLS_DIR}" fetch origin --prune

    local depot_tools_branch
    depot_tools_branch="$(
      git -C "${DEPOT_TOOLS_DIR}" symbolic-ref --quiet --short refs/remotes/origin/HEAD 2>/dev/null | \
        sed 's#^origin/##'
    )"
    if [[ -z "${depot_tools_branch}" ]]; then
      depot_tools_branch="main"
    fi

    if git -C "${DEPOT_TOOLS_DIR}" rev-parse --verify "${depot_tools_branch}" >/dev/null 2>&1; then
      git -C "${DEPOT_TOOLS_DIR}" switch "${depot_tools_branch}"
    else
      git -C "${DEPOT_TOOLS_DIR}" switch --track -c "${depot_tools_branch}" "origin/${depot_tools_branch}"
    fi

    git -C "${DEPOT_TOOLS_DIR}" merge --ff-only "origin/${depot_tools_branch}"
  fi
  prepend_depot_tools_path
  require_command fetch "depot_tools did not provide fetch."
  require_command gclient "depot_tools did not provide gclient."
  require_command gn "depot_tools did not provide gn."
  require_command autoninja "depot_tools did not provide autoninja."
}

ensure_homebrew() {
  prepend_common_mac_paths
  if command_exists brew; then
    return 0
  fi

  require_command curl "curl is required to install Homebrew."
  log "Installing Homebrew"
  if [[ -t 0 && -t 1 ]]; then
    /bin/bash -c \
      "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
  else
    NONINTERACTIVE=1 /bin/bash -c \
      "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
  fi

  prepend_common_mac_paths
  require_command brew \
    "Homebrew install completed but brew is not on PATH. Expected /opt/homebrew/bin/brew or /usr/local/bin/brew."
}

ensure_ccache() {
  prepend_common_mac_paths

  if ! command_exists ccache; then
    ensure_homebrew
    log "Installing ccache with Homebrew"
    brew install ccache
  fi

  CCACHE_BIN="$(command -v ccache)"
  [[ -n "${CCACHE_BIN}" ]] || die "ccache was expected in PATH but was not resolved."
  assert_no_spaces "${CCACHE_BIN}" "ccache binary path"

  mkdir -p "${CCACHE_DIR}"
  env CCACHE_DIR="${CCACHE_DIR}" ccache -M "${CCACHE_MAX_SIZE}" >/dev/null
}

show_ccache_stats() {
  prepend_common_mac_paths
  require_command ccache "Install ccache first."
  env CCACHE_DIR="${CCACHE_DIR}" ccache -s
}

set_ccache_size() {
  prepend_common_mac_paths
  require_command ccache "Install ccache first."
  mkdir -p "${CCACHE_DIR}"
  env CCACHE_DIR="${CCACHE_DIR}" ccache -M "${CCACHE_MAX_SIZE}"
}

normalize_target() {
  TARGET="${TARGET#//}"
}

chromium_src_dir() {
  printf '%s\n' "${CHROMIUM_DIR}/src"
}

ensure_chromium_checkout_exists() {
  local src_dir
  src_dir="$(chromium_src_dir)"
  if ! git -C "${src_dir}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    die "Chromium checkout not found at ${src_dir}. Run the checkout command first."
  fi
}

load_chromium_pin() {
  if [[ -n "${CHROMIUM_REVISION}" ]]; then
    [[ -n "${CHROMIUM_VERSION_LABEL}" ]] || CHROMIUM_VERSION_LABEL="override"
    return 0
  fi

  [[ -f "${CHROMIUM_PIN_FILE}" ]] || \
    die "Chromium pin file not found: ${CHROMIUM_PIN_FILE}"

  # shellcheck disable=SC1090
  source "${CHROMIUM_PIN_FILE}"

  [[ -n "${CHROMIUM_REVISION}" ]] || \
    die "Chromium pin file did not set CHROMIUM_REVISION: ${CHROMIUM_PIN_FILE}"

  [[ -n "${CHROMIUM_VERSION_LABEL}" ]] || \
    CHROMIUM_VERSION_LABEL="${CHROMIUM_REVISION}"
}

ensure_chromium_revision_mode() {
  load_chromium_pin

  [[ -z "${CHROMIUM_BRANCH}" ]] || \
    die "--chromium-branch is incompatible with pinned revision mode. Use --chromium-revision <sha> to override the pin."
}

ensure_git_remote_url() {
  local repo="$1"
  local remote="$2"
  local url="$3"

  if git -C "${repo}" remote get-url "${remote}" >/dev/null 2>&1; then
    local current_url
    current_url="$(git -C "${repo}" remote get-url "${remote}")"
    if [[ "${current_url}" != "${url}" ]]; then
      git -C "${repo}" remote set-url "${remote}" "${url}"
    fi
  else
    git -C "${repo}" remote add "${remote}" "${url}"
  fi
}

ensure_branch_checked_out() {
  local repo="$1"
  local remote="$2"
  local branch="$3"

  [[ -z "${branch}" ]] && return 0

  require_git_repo "${repo}"
  require_clean_repo "${repo}"

  git -C "${repo}" fetch "${remote}" --prune

  if git -C "${repo}" rev-parse --verify "${branch}" >/dev/null 2>&1; then
    git -C "${repo}" switch "${branch}"
    return 0
  fi

  if git -C "${repo}" show-ref --verify --quiet "refs/remotes/${remote}/${branch}"; then
    git -C "${repo}" switch --track -c "${branch}" "${remote}/${branch}"
    return 0
  fi

  die "Branch ${branch} not found in ${repo} via remote ${remote}."
}

resolve_project_dir() {
  if [[ -n "${PROJECT_REPO_URL}" ]]; then
    if [[ "${PROJECT_DIR}" == "${DEFAULT_PROJECT_DIR}" ]]; then
      PROJECT_DIR="${HOME}/work/clawbrowser-source"
    fi

    if [[ ! -d "${PROJECT_DIR}/.git" ]]; then
      log "Cloning project repository into ${PROJECT_DIR}"
      mkdir -p "$(dirname "${PROJECT_DIR}")"
      git clone "${PROJECT_REPO_URL}" "${PROJECT_DIR}"
    fi

    ensure_git_remote_url "${PROJECT_DIR}" "${PROJECT_REMOTE}" "${PROJECT_REPO_URL}"
    git -C "${PROJECT_DIR}" fetch "${PROJECT_REMOTE}" --prune

    if [[ -n "${PROJECT_BRANCH}" ]]; then
      ensure_branch_checked_out "${PROJECT_DIR}" "${PROJECT_REMOTE}" "${PROJECT_BRANCH}"
      git -C "${PROJECT_DIR}" pull --ff-only "${PROJECT_REMOTE}" "${PROJECT_BRANCH}"
    fi
  else
    require_git_repo "${PROJECT_DIR}"
    if [[ -n "${PROJECT_BRANCH}" ]]; then
      ensure_branch_checked_out "${PROJECT_DIR}" "${PROJECT_REMOTE}" "${PROJECT_BRANCH}"
    fi
  fi
}

ensure_clawbrowser_resource_ids() {
  local spec_file
  spec_file="$(chromium_src_dir)/tools/gritsettings/resource_ids.spec"

  [[ -f "${spec_file}" ]] || \
    die "Chromium resource id spec not found: ${spec_file}"

  python3 - "${spec_file}" <<'PY'
from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
target = "clawbrowser/verify/clawbrowser_verify.grd"
entry = [
    f'  "{target}": {{\n',
    '    "includes": [9310],\n',
    '  },\n',
    '\n',
]

text = path.read_text()
if f'"{target}"' in text:
    raise SystemExit(0)

lines = text.splitlines(keepends=True)
try:
    start = next(
        i for i, line in enumerate(lines)
        if '# START "everything else" section.' in line
    )
    end = next(
        i for i, line in enumerate(lines)
        if '# END "everything else" section.' in line
    )
except StopIteration as exc:
    raise SystemExit(
        f"Could not find the expected section markers in {path}"
    ) from exc

path_pattern = re.compile(r'^  "([^"]+)": \{\n$')
insert_at = end
for i in range(start + 1, end):
    match = path_pattern.match(lines[i])
    if not match:
        continue
    if match.group(1) > target:
        insert_at = i
        break

lines[insert_at:insert_at] = entry
path.write_text("".join(lines))
PY

  log "Ensured clawbrowser resource IDs in ${spec_file}"
}

sync_chromium_branding_assets() {
  local compile_record_path="${1:-}"
  local src_dir
  local icon_dir
  local chromium_theme_dir
  local chromium_mac_dir
  local chromium_asset_catalog_dir
  local chromium_appicon_dir
  local chromium_iconset_dir
  local chromium_default_100_dir
  local asset

  src_dir="$(chromium_src_dir)"
  icon_dir="${PROJECT_DIR}/branding/icons/app/side-bite"
  chromium_theme_dir="${src_dir}/chrome/app/theme/chromium"
  chromium_mac_dir="${chromium_theme_dir}/mac"
  chromium_asset_catalog_dir="${chromium_mac_dir}/Assets.xcassets"
  chromium_appicon_dir="${chromium_asset_catalog_dir}/AppIcon.appiconset"
  chromium_iconset_dir="${chromium_asset_catalog_dir}/Icon.iconset"
  chromium_default_100_dir="${src_dir}/chrome/app/theme/default_100_percent/chromium"

  [[ -f "${icon_dir}/app.icns" ]] || die "Missing branding icon: ${icon_dir}/app.icns"
  for asset in 16 22 24 32 48 64 128 256 512 1024; do
    [[ -f "${icon_dir}/product_logo_${asset}.png" ]] || \
      die "Missing branding icon: ${icon_dir}/product_logo_${asset}.png"
  done

  mkdir -p "${chromium_theme_dir}" \
           "${chromium_mac_dir}" \
           "${chromium_appicon_dir}" \
           "${chromium_iconset_dir}" \
           "${chromium_default_100_dir}"

  cp "${icon_dir}/app.icns" "${chromium_mac_dir}/app.icns"
  for asset in 16 22 24 32 48 64 128 256 512 1024; do
    cp "${icon_dir}/product_logo_${asset}.png" \
      "${chromium_theme_dir}/product_logo_${asset}.png"
  done
  cp "${icon_dir}/product_logo_32.png" \
    "${chromium_default_100_dir}/product_logo_32.png"

  cat >"${chromium_asset_catalog_dir}/Contents.json" <<'EOF'
{
  "info" : {
    "author" : "xcode",
    "version" : 1
  }
}
EOF

  cat >"${chromium_appicon_dir}/Contents.json" <<'EOF'
{
  "images" : [
    { "filename" : "appicon_16.png", "idiom" : "mac", "scale" : "1x", "size" : "16x16" },
    { "filename" : "appicon_32.png", "idiom" : "mac", "scale" : "2x", "size" : "16x16" },
    { "filename" : "appicon_32.png", "idiom" : "mac", "scale" : "1x", "size" : "32x32" },
    { "filename" : "appicon_64.png", "idiom" : "mac", "scale" : "2x", "size" : "32x32" },
    { "filename" : "appicon_128.png", "idiom" : "mac", "scale" : "1x", "size" : "128x128" },
    { "filename" : "appicon_256.png", "idiom" : "mac", "scale" : "2x", "size" : "128x128" },
    { "filename" : "appicon_256.png", "idiom" : "mac", "scale" : "1x", "size" : "256x256" },
    { "filename" : "appicon_512.png", "idiom" : "mac", "scale" : "2x", "size" : "256x256" },
    { "filename" : "appicon_512.png", "idiom" : "mac", "scale" : "1x", "size" : "512x512" },
    { "filename" : "appicon_1024.png", "idiom" : "mac", "scale" : "2x", "size" : "512x512" }
  ],
  "info" : {
    "author" : "xcode",
    "version" : 1
  }
}
EOF

  cp "${icon_dir}/product_logo_16.png" "${chromium_appicon_dir}/appicon_16.png"
  cp "${icon_dir}/product_logo_32.png" "${chromium_appicon_dir}/appicon_32.png"
  cp "${icon_dir}/product_logo_64.png" "${chromium_appicon_dir}/appicon_64.png"
  cp "${icon_dir}/product_logo_128.png" "${chromium_appicon_dir}/appicon_128.png"
  cp "${icon_dir}/product_logo_256.png" "${chromium_appicon_dir}/appicon_256.png"
  cp "${icon_dir}/product_logo_512.png" "${chromium_appicon_dir}/appicon_512.png"
  cp "${icon_dir}/product_logo_1024.png" "${chromium_appicon_dir}/appicon_1024.png"
  cp "${icon_dir}/product_logo_256.png" "${chromium_iconset_dir}/icon_256x256.png"
  cp "${icon_dir}/product_logo_512.png" "${chromium_iconset_dir}/icon_256x256@2x.png"

  if [[ -f "${src_dir}/tools/mac/icons/compile_car.py" ]]; then
    if [[ -n "${compile_record_path}" ]]; then
      python3 "${src_dir}/tools/mac/icons/compile_car.py" \
        "chrome/app/theme/chromium/mac/Assets.xcassets" \
        "${compile_record_path}"
    elif [[ "$(uname -s)" == "Darwin" ]]; then
      (
        cd "${src_dir}"
        python3 tools/mac/icons/compile_car.py \
          chrome/app/theme/chromium/mac/Assets.xcassets
      )
    fi
  fi

  log "Synced Clawbrowser branding into Chromium app theme assets"
}

sync_project_overlay() {
  local src_dir
  src_dir="$(chromium_src_dir)"

  ensure_chromium_checkout_exists
  resolve_project_dir

  log "Syncing project overlay from ${PROJECT_DIR} into ${src_dir}"
  mkdir -p "${src_dir}/clawbrowser"
  rsync -a --delete "${PROJECT_DIR}/clawbrowser/" "${src_dir}/clawbrowser/"

  mkdir -p "${src_dir}/api"
  rsync -a "${PROJECT_DIR}/api/openapi.yaml" "${src_dir}/api/openapi.yaml"
  sync_chromium_branding_assets
  ensure_clawbrowser_resource_ids
  disable_google_api_keys_infobar
}

disable_google_api_keys_infobar() {
  local src_dir
  local infobar_path

  src_dir="$(chromium_src_dir)"
  infobar_path="${src_dir}/chrome/browser/ui/startup/infobar_utils.cc"

  [[ -f "${infobar_path}" ]] || \
    die "Missing startup infobar source: ${infobar_path}"

  python3 - "${infobar_path}" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
marker = "Clawbrowser intentionally ships without Google API keys"
source = path.read_text()
needle = """  if (!google_apis::HasAPIKeyConfigured()) {
    GoogleApiKeysInfoBarDelegate::Create(infobar_manager);
  }
"""
replacement = """  // Clawbrowser intentionally ships without Google API keys; avoid Chromium's missing-keys
  // startup infobar while keeping those keys unset.
"""

if marker in source:
    sys.exit(0)

if needle not in source:
    raise SystemExit(f"Could not find Google API keys infobar block in {path}")

path.write_text(source.replace(needle, replacement))
PY
}

collect_patch_manifests() {
  local -a patches=()
  shopt -s nullglob
  patches=("${PROJECT_DIR}"/clawbrowser/patches/[0-9][0-9][0-9]-*.patch)
  shopt -u nullglob

  ((${#patches[@]} > 0)) || \
    die "No patch manifests found under ${PROJECT_DIR}/clawbrowser/patches"

  printf '%s\n' "${patches[@]}"
}

collect_patch_targets() {
  python3 - "${PROJECT_DIR}" <<'PY'
from pathlib import Path
import sys

project_dir = Path(sys.argv[1])
patch_dir = project_dir / "clawbrowser" / "patches"
paths = set()

for patch in sorted(patch_dir.glob("[0-9][0-9][0-9]-*.patch")):
    for line in patch.read_text().splitlines():
        if not (line.startswith("--- ") or line.startswith("+++ ")):
            continue
        path = line.split(" ", 1)[1]
        if path == "/dev/null":
            continue
        if path.startswith("a/") or path.startswith("b/"):
            path = path[2:]
        paths.add(path)

for path in sorted(paths):
    print(path)
PY
}

patch_targets_overlay_only() {
  python3 - "$1" <<'PY'
from pathlib import Path
import sys

patch = Path(sys.argv[1])
paths = set()

for line in patch.read_text().splitlines():
    if not (line.startswith("--- ") or line.startswith("+++ ")):
        continue
    path = line.split(" ", 1)[1]
    if path == "/dev/null":
        continue
    if path.startswith("a/") or path.startswith("b/"):
        path = path[2:]
    paths.add(path)

overlay_prefixes = ("clawbrowser/", "api/")
if paths and all(path.startswith(overlay_prefixes) for path in paths):
    print("yes")
PY
}

reset_patch_targets_to_pin() {
  local src_dir
  local actual_revision
  local path
  local -a restore_paths=()
  local -a remove_paths=()
  local -a restore_v8_paths=()
  local -a remove_v8_paths=()

  src_dir="$(chromium_src_dir)"

  ensure_chromium_revision_mode
  actual_revision="$(git -C "${src_dir}" rev-parse HEAD)"
  if [[ "${actual_revision}" != "${CHROMIUM_REVISION}" ]]; then
    die "Chromium checkout is at ${actual_revision}, expected ${CHROMIUM_REVISION}. Run checkout/update first."
  fi

  while IFS= read -r path; do
    [[ -n "${path}" ]] || continue
    if [[ "${path}" == v8/* && -d "${src_dir}/v8/.git" ]]; then
      local v8_path="${path#v8/}"
      if git -C "${src_dir}/v8" cat-file -e "HEAD:${v8_path}" >/dev/null 2>&1; then
        restore_v8_paths+=("${v8_path}")
      else
        remove_v8_paths+=("${v8_path}")
      fi
      continue
    fi

    if git -C "${src_dir}" cat-file -e "${CHROMIUM_REVISION}:${path}" >/dev/null 2>&1; then
      restore_paths+=("${path}")
    else
      remove_paths+=("${path}")
    fi
  done < <(collect_patch_targets)

  if ((${#restore_paths[@]} > 0)); then
    log "Resetting ${#restore_paths[@]} patch-owned paths to ${CHROMIUM_VERSION_LABEL} (${CHROMIUM_REVISION})"
    git -C "${src_dir}" restore --source "${CHROMIUM_REVISION}" --worktree -- "${restore_paths[@]}"
  fi

  if ((${#remove_paths[@]} > 0)); then
    log "Removing ${#remove_paths[@]} patch-owned paths absent from ${CHROMIUM_VERSION_LABEL} (${CHROMIUM_REVISION})"
    (
      cd "${src_dir}"
      rm -f -- "${remove_paths[@]}"
    )
  fi

  if ((${#restore_v8_paths[@]} > 0)); then
    log "Resetting ${#restore_v8_paths[@]} V8 patch-owned paths to nested V8 HEAD"
    git -C "${src_dir}/v8" restore --source HEAD --worktree -- "${restore_v8_paths[@]}"
  fi

  if ((${#remove_v8_paths[@]} > 0)); then
    log "Removing ${#remove_v8_paths[@]} V8 patch-owned paths absent from nested V8 HEAD"
    (
      cd "${src_dir}/v8"
      rm -f -- "${remove_v8_paths[@]}"
    )
  fi
}

apply_repo_patches() {
  local src_dir
  local patch

  src_dir="$(chromium_src_dir)"

  ensure_chromium_checkout_exists
  ensure_patch_prereqs
  resolve_project_dir
  reset_patch_targets_to_pin
  sync_project_overlay

  while IFS= read -r patch; do
    if [[ "$(patch_targets_overlay_only "${patch}")" == "yes" ]]; then
      log "Patch covered by overlay sync: $(basename "${patch}")"
      continue
    fi

    if (
      cd "${src_dir}" &&
      git apply --reverse --check "${patch}" >/dev/null 2>&1
    ); then
      log "Patch already applied: $(basename "${patch}")"
      continue
    fi

    log "Applying patch: $(basename "${patch}")"
    (
      cd "${src_dir}"
      git apply "${patch}"
    )
  done < <(collect_patch_manifests)
}

write_args_gn() {
  local args_file
  args_file="${BUILD_DIR}/args.gn"

  ensure_ccache
  assert_no_spaces "${CHROMIUM_DIR}" "Chromium checkout path"
  assert_no_spaces "${BUILD_DIR}" "Build directory path"
  assert_no_spaces "${CCACHE_DIR}" "ccache directory path"

  mkdir -p "${BUILD_DIR}"

  cat >"${args_file}" <<EOF
is_debug = false
is_component_build = true
symbol_level = 0
root_extra_deps = [ "//clawbrowser" ]
EOF

  cat >>"${args_file}" <<EOF
cc_wrapper = "env CCACHE_DIR=${CCACHE_DIR} CCACHE_SLOPPINESS=time_macros ${CCACHE_BIN}"
EOF

  log "Wrote ${args_file}"
}

run_gn_gen() {
  local src_dir
  src_dir="$(chromium_src_dir)"
  ensure_chromium_checkout_exists
  ensure_base_prereqs
  ensure_depot_tools
  ensure_ccache

  (
    cd "${src_dir}"
    gn gen "${BUILD_DIR}"
  )
}

build_target() {
  local src_dir
  src_dir="$(chromium_src_dir)"

  ensure_chromium_checkout_exists
  ensure_base_prereqs
  ensure_depot_tools
  ensure_ccache
  normalize_target

  (
    cd "${src_dir}"
    autoninja -C "${BUILD_DIR}" "${TARGET}"
  )
}

require_dev_icon_asset() {
  local asset_name="$1"
  local asset_path="${PROJECT_DIR}/branding/icons/app/side-bite/${asset_name}"

  [[ -f "${asset_path}" ]] || \
    die "Missing branding icon: ${asset_path}"
}

write_dev_linux_wrapper() {
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

overlay_dev_linux_bundle_icons() {
  local bundle_dir="$1"
  local size

  for size in 16 22 24 32 48 64 128 256 512 1024; do
    require_dev_icon_asset "product_logo_${size}.png"
    cp "${PROJECT_DIR}/branding/icons/app/side-bite/product_logo_${size}.png" \
      "${bundle_dir}/product_logo_${size}.png"
  done

  if [[ -d "${bundle_dir}/resources" ]]; then
    for size in 16 22 24 32 48 64 128 256 512 1024; do
      cp "${PROJECT_DIR}/branding/icons/app/side-bite/product_logo_${size}.png" \
        "${bundle_dir}/resources/product_logo_${size}.png"
    done
  fi
}

rewrite_dev_macos_bundle_metadata() {
  local staged_app="$1"
  local plist_path="${staged_app}/Contents/Info.plist"

  [[ -f "${plist_path}" ]] || \
    die "Missing macOS bundle Info.plist: ${plist_path}"

  python3 - "${staged_app}" <<'PY'
from pathlib import Path
import plistlib
import sys

APP_NAME = "Clawbrowser"
SOURCE_APP_NAME = "Chromium"
BUNDLE_ID = "ai.clawbrowser.Clawbrowser"
BUNDLE_VERSION = "1.0.0"
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

overlay_dev_macos_app_icons() {
  local staged_app="$1"
  local icon_dir="${PROJECT_DIR}/branding/icons/app/side-bite"
  local resources_dir="${staged_app}/Contents/Resources"

  [[ -d "${resources_dir}" ]] || \
    die "Missing macOS bundle resources dir: ${resources_dir}"
  require_dev_icon_asset "app.icns"
  require_dev_icon_asset "product_logo_32.png"

  cp "${icon_dir}/app.icns" "${resources_dir}/app.icns"
  cp "${icon_dir}/product_logo_32.png" "${resources_dir}/product_logo_32.png"
  rm -f "${resources_dir}/Assets.car"
}

copy_dev_macos_runtime_dylibs() {
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

sign_dev_macos_app_bundle() {
  local staged_app="$1"

  codesign --force --deep --sign - "${staged_app}"
}

stage_dev_macos_browser_bundle() {
  local source_app="${BUILD_DIR}/Chromium.app"
  local staged_app="${BUILD_DIR}/Clawbrowser.app"
  local chromium_binary_path="${staged_app}/Contents/MacOS/Chromium"
  local clawbrowser_binary_path="${staged_app}/Contents/MacOS/Clawbrowser"

  [[ -d "${source_app}" ]] || \
    die "Built Chromium.app not found under ${BUILD_DIR}. Run the build command first."

  rsync -a --delete "${source_app}/" "${staged_app}/"
  [[ -x "${chromium_binary_path}" ]] || \
    die "Built Chromium executable not found: ${chromium_binary_path}"
  mv "${chromium_binary_path}" "${clawbrowser_binary_path}"
  copy_dev_macos_runtime_dylibs "${BUILD_DIR}" "${staged_app}"
  rewrite_dev_macos_bundle_metadata "${staged_app}"
  overlay_dev_macos_app_icons "${staged_app}"
  sign_dev_macos_app_bundle "${staged_app}"
  log "Staged runnable Clawbrowser app at ${staged_app}"
}

stage_dev_linux_browser_bundle() {
  local chrome_binary_path="${BUILD_DIR}/chrome"
  local real_binary_path="${BUILD_DIR}/clawbrowser.real"
  local wrapper_path="${BUILD_DIR}/clawbrowser"

  if [[ -x "${chrome_binary_path}" ]]; then
    mv -f "${chrome_binary_path}" "${real_binary_path}"
  elif [[ ! -x "${real_binary_path}" ]]; then
    die "Built chrome binary not found under ${BUILD_DIR}. Run the build command first."
  fi

  overlay_dev_linux_bundle_icons "${BUILD_DIR}"
  write_dev_linux_wrapper "${wrapper_path}"
  log "Staged runnable clawbrowser bundle at ${BUILD_DIR}"
}

stage_dev_browser_bundle() {
  local host_os="${1:-$(uname -s)}"

  if [[ "${TARGET}" != "chrome" ]]; then
    return 0
  fi

  case "${host_os}" in
    Darwin)
      stage_dev_macos_browser_bundle
      ;;
    Linux)
      stage_dev_linux_browser_bundle
      ;;
    *)
      log "Skipping Clawbrowser dev bundle staging for unsupported OS: ${host_os}"
      ;;
  esac
}

integration_requirements_file() {
  printf '%s\n' "$(chromium_src_dir)/clawbrowser/test/integration/requirements.txt"
}

integration_browser_binary() {
  local candidate
  local checked_binaries
  local -a candidates=(
    "${BUILD_DIR}/Clawbrowser.app/Contents/MacOS/Clawbrowser"
    "${BUILD_DIR}/clawbrowser"
  )

  for candidate in "${candidates[@]}"; do
    if [[ -x "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  checked_binaries="$(printf '%s\n' "${candidates[@]}")"
  die "Built browser binary not found under ${BUILD_DIR}. Checked:
${checked_binaries}"
}

setup_integration_env() {
  local requirements_file
  local python_bin

  ensure_chromium_checkout_exists
  ensure_base_prereqs
  resolve_project_dir
  sync_project_overlay
  assert_no_spaces "${INTEGRATION_VENV_DIR}" "Integration venv path"

  requirements_file="$(integration_requirements_file)"
  [[ -f "${requirements_file}" ]] || \
    die "Integration requirements file not found: ${requirements_file}"

  mkdir -p "$(dirname "${INTEGRATION_VENV_DIR}")"
  if [[ ! -x "${INTEGRATION_VENV_DIR}/bin/python3" ]]; then
    log "Creating integration venv at ${INTEGRATION_VENV_DIR}"
    python3 -m venv "${INTEGRATION_VENV_DIR}"
  fi

  python_bin="${INTEGRATION_VENV_DIR}/bin/python3"
  "${python_bin}" -m pip install --upgrade pip
  "${python_bin}" -m pip install -r "${requirements_file}"
}

run_integration_tests() {
  local src_dir
  local python_bin
  local browser_binary

  src_dir="$(chromium_src_dir)"

  ensure_chromium_checkout_exists
  setup_integration_env

  python_bin="${INTEGRATION_VENV_DIR}/bin/python3"
  browser_binary="$(integration_browser_binary)"

  (
    cd "${src_dir}"
    CLAWBROWSER_BINARY="${browser_binary}" \
      CLAWBROWSER_PROJECT_DIR="${PROJECT_DIR}" \
      "${python_bin}" clawbrowser/test/integration/run_integration_tests.py
  )
}

sync_chromium_revision() {
  local src_dir
  local actual_revision

  src_dir="$(chromium_src_dir)"

  ensure_chromium_checkout_exists
  ensure_chromium_revision_mode
  require_clean_repo "${src_dir}"

  log "Syncing Chromium checkout to ${CHROMIUM_VERSION_LABEL} (${CHROMIUM_REVISION})"
  (
    cd "${CHROMIUM_DIR}"
    gclient sync -D --force --reset --revision "src@${CHROMIUM_REVISION}"
  )

  actual_revision="$(git -C "${src_dir}" rev-parse HEAD)"
  if [[ "${actual_revision}" != "${CHROMIUM_REVISION}" ]]; then
    die "Chromium checkout resolved to ${actual_revision}, expected ${CHROMIUM_REVISION}."
  fi
}

checkout_chromium() {
  local fetch_cmd
  local src_dir
  src_dir="$(chromium_src_dir)"

  ensure_base_prereqs
  ensure_depot_tools
  ensure_ccache
  assert_no_spaces "${CHROMIUM_DIR}" "Chromium checkout path"

  if git -C "${src_dir}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    log "Chromium checkout already exists at ${src_dir}"
    sync_chromium_revision
    return 0
  fi

  mkdir -p "${CHROMIUM_DIR}"
  if [[ -n "$(find "${CHROMIUM_DIR}" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null)" ]]; then
    die "Chromium directory is not empty: ${CHROMIUM_DIR}"
  fi

  if ((FETCH_NO_HISTORY)); then
    fetch_cmd=(fetch --no-history chromium)
  else
    fetch_cmd=(fetch chromium)
  fi

  log "Running Chromium fetch in ${CHROMIUM_DIR}"
  (
    cd "${CHROMIUM_DIR}"
    if command_exists caffeinate; then
      caffeinate "${fetch_cmd[@]}"
    else
      "${fetch_cmd[@]}"
    fi
  )

  sync_chromium_revision
  tune_git_checkout
}

update_chromium() {
  ensure_chromium_checkout_exists
  ensure_base_prereqs
  ensure_depot_tools

  sync_chromium_revision
  tune_git_checkout
}

tune_git_checkout() {
  local src_dir
  local git_version
  local untracked_cache_test

  src_dir="$(chromium_src_dir)"
  ensure_chromium_checkout_exists

  (
    cd "${src_dir}"

    untracked_cache_test="$(git update-index --test-untracked-cache 2>&1 || true)"
    if [[ "${untracked_cache_test}" == *"OK"* ]]; then
      git config core.untrackedCache true
      log "Enabled git core.untrackedCache in ${src_dir}"
    else
      log "Skipping git core.untrackedCache; test did not report OK"
    fi

    git_version="$(git version | awk '{print $3}')"
    if version_ge "${git_version}" "2.43.0"; then
      git config core.fsmonitor true
      log "Enabled git core.fsmonitor in ${src_dir}"
    else
      log "Skipping git core.fsmonitor; git ${git_version} is older than 2.43.0"
    fi
  )
}

machine_setup() {
  case "$(uname -s)" in
    Darwin)
      ensure_base_prereqs
      ;;
    Linux)
      ensure_patch_prereqs
      ;;
    *)
      die "Unsupported host OS for machine-setup: $(uname -s)"
      ;;
  esac
  ensure_depot_tools
  ensure_ccache

  log "Machine setup complete"
  log "depot_tools: ${DEPOT_TOOLS_DIR}"
  log "ccache dir: ${CCACHE_DIR}"
  log "ccache max size: ${CCACHE_MAX_SIZE}"
}

parse_args() {
  while (($# > 0)); do
    case "$1" in
      --depot-tools-dir)
        DEPOT_TOOLS_DIR="$2"
        shift 2
        ;;
      --chromium-dir)
        CHROMIUM_DIR="$2"
        shift 2
        ;;
      --build-dir)
        BUILD_DIR="$2"
        shift 2
        ;;
      --project-dir)
        PROJECT_DIR="$2"
        shift 2
        ;;
      --project-repo-url)
        PROJECT_REPO_URL="$2"
        shift 2
        ;;
      --project-branch)
        PROJECT_BRANCH="$2"
        shift 2
        ;;
      --project-remote)
        PROJECT_REMOTE="$2"
        shift 2
        ;;
      --chromium-pin-file)
        CHROMIUM_PIN_FILE="$2"
        shift 2
        ;;
      --chromium-revision)
        CHROMIUM_REVISION="$2"
        shift 2
        ;;
      --chromium-branch)
        CHROMIUM_BRANCH="$2"
        shift 2
        ;;
      --chromium-remote)
        CHROMIUM_REMOTE="$2"
        shift 2
        ;;
      --target)
        TARGET="$2"
        shift 2
        ;;
      --integration-venv-dir)
        INTEGRATION_VENV_DIR="$2"
        shift 2
        ;;
      --ccache-dir)
        CCACHE_DIR="$2"
        shift 2
        ;;
      --ccache-max-size)
        CCACHE_MAX_SIZE="$2"
        shift 2
        ;;
      --with-history)
        FETCH_NO_HISTORY=0
        shift
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

main() {
  local command

  if (($# == 0)); then
    usage
    exit 1
  fi

  command="$1"
  shift
  parse_args "$@"

  case "${command}" in
    machine-setup)
      machine_setup
      ;;
    checkout)
      checkout_chromium
      ;;
    update)
      update_chromium
      ;;
    sync-project)
      sync_project_overlay
      ;;
    apply-patches)
      apply_repo_patches
      ;;
    gen)
      write_args_gn
      run_gn_gen
      ;;
    build)
      sync_project_overlay
      write_args_gn
      run_gn_gen
      build_target
      stage_dev_browser_bundle
      ;;
    integration-setup)
      setup_integration_env
      ;;
    integration-test)
      run_integration_tests
      ;;
    ccache-stats)
      show_ccache_stats
      ;;
    ccache-size)
      set_ccache_size
      ;;
    tune-git)
      tune_git_checkout
      ;;
    help)
      usage
      ;;
    *)
      die "Unknown command: ${command}"
      ;;
  esac
}

main "$@"
