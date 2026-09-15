#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

DEFAULT_PROJECT_DIR="${REPO_ROOT}"
DEFAULT_CHROMIUM_PIN_FILE="${SCRIPT_DIR}/chromium_pin.conf"

DEPOT_TOOLS_DIR="${DEPOT_TOOLS_DIR:-${HOME}/opt/depot_tools}"
CHROMIUM_DIR="${CHROMIUM_DIR:-${HOME}/work/chromium}"
CCACHE_DIR="${CCACHE_DIR:-${HOME}/cache/ccache}"
CCACHE_MAX_SIZE="${CCACHE_MAX_SIZE:-50G}"
CCACHE_BIN="${CCACHE_BIN:-}"
BUILD_DIR="${BUILD_DIR:-${CHROMIUM_DIR}/src/out/CBFast}"
TARGET="${TARGET:-chrome}"
INTEGRATION_VENV_DIR="${INTEGRATION_VENV_DIR:-${HOME}/.cache/clawbrowser/integration-venv}"

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
  scripts/chromium_remote.sh <command> [options]

Commands:
  machine-setup     Check macOS/Xcode prerequisites, install MetalToolchain, depot_tools, Homebrew/ccache
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
  --chromium-pin-file PATH Default: scripts/chromium_pin.conf
  --chromium-revision SHA  Override the repo-pinned Chromium revision for this run
  --chromium-branch NAME   Legacy branch checkout flow; incompatible with pinned revision mode
  --chromium-remote NAME   Default: origin
  --target NAME            Default: chrome
  --integration-venv-dir PATH Default: ~/.cache/clawbrowser/integration-venv
  --ccache-dir PATH        Default: ~/cache/ccache
  --ccache-max-size SIZE   Default: 50G
  --with-history           Use fetch chromium instead of fetch --no-history chromium

Examples:
  bash scripts/chromium_remote.sh machine-setup
  bash scripts/chromium_remote.sh checkout
  bash scripts/chromium_remote.sh checkout --chromium-revision 25a94a5662bc9cce918f4626472e7edca1ba2b39
  bash scripts/chromium_remote.sh apply-patches
  bash scripts/chromium_remote.sh build --target chrome
  bash scripts/chromium_remote.sh integration-setup
  bash scripts/chromium_remote.sh integration-test
  bash scripts/chromium_remote.sh build --target //clawbrowser:clawbrowser_unittests
  bash scripts/chromium_remote.sh build --project-repo-url git@github.com:you/clawbrowser.git --project-branch browser-component-impl
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
  ensure_clawbrowser_resource_ids
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

patch_creates_target() {
  python3 - "${PROJECT_DIR}" "$1" <<'PY'
from pathlib import Path, PurePosixPath
import sys

target = sys.argv[2]
if PurePosixPath(target).is_absolute() or ".." in PurePosixPath(target).parts:
    raise SystemExit(1)
for patch in sorted((Path(sys.argv[1]) / "clawbrowser" / "patches").glob("[0-9][0-9][0-9]-*.patch")):
    old = None
    for line in patch.read_text().splitlines():
        if line.startswith("--- "):
            old = line[4:]
        elif line.startswith("+++ "):
            new = line[4:]
            if new.startswith("b/"):
                new = new[2:]
            if old == "/dev/null" and new == target:
                raise SystemExit(0)
raise SystemExit(1)
PY
}

reset_patch_targets_to_pin() {
  local src_dir
  local actual_revision
  local path
  local -a restore_paths=()
  local -a remove_paths=()

  src_dir="$(chromium_src_dir)"

  ensure_chromium_revision_mode
  actual_revision="$(git -C "${src_dir}" rev-parse HEAD)"
  if [[ "${actual_revision}" != "${CHROMIUM_REVISION}" ]]; then
    die "Chromium checkout is at ${actual_revision}, expected ${CHROMIUM_REVISION}. Run checkout/update first."
  fi

  while IFS= read -r path; do
    [[ -n "${path}" ]] || continue
    if [[ "${path}" == third_party/skia/* ]]; then
      local skia_dir="${src_dir}/third_party/skia"
      local skia_path="${path#third_party/skia/}"
      [[ -e "${skia_dir}/.git" ]] || die "Missing nested Skia checkout"
      if git -C "${skia_dir}" cat-file -e "HEAD:${skia_path}" >/dev/null 2>&1; then
        git -C "${skia_dir}" restore --source HEAD --worktree -- "${skia_path}"
      elif patch_creates_target "${path}"; then
        log "Removing patch-created Skia file: ${skia_path}"
        rm -f -- "${skia_dir}/${skia_path}"
      else
        die "Skia patch target is absent from nested HEAD and not declared new: ${skia_path}"
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
}

apply_repo_patches() {
  local src_dir
  local patch

  src_dir="$(chromium_src_dir)"

  ensure_chromium_checkout_exists
  ensure_base_prereqs
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

integration_requirements_file() {
  printf '%s\n' "$(chromium_src_dir)/clawbrowser/test/integration/requirements.txt"
}

integration_browser_binary() {
  local mac_binary
  local linux_binary

  mac_binary="${BUILD_DIR}/Chromium.app/Contents/MacOS/Chromium"
  linux_binary="${BUILD_DIR}/chrome"

  if [[ -x "${mac_binary}" ]]; then
    printf '%s\n' "${mac_binary}"
    return 0
  fi

  if [[ -x "${linux_binary}" ]]; then
    printf '%s\n' "${linux_binary}"
    return 0
  fi

  die "Built browser binary not found under ${BUILD_DIR}. Run the build command first."
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
  ensure_base_prereqs
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
