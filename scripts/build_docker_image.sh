#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DOCKERFILE_PATH="${REPO_ROOT}/docker/clawbrowser-runtime.Dockerfile"
ENTRYPOINT_PATH="${REPO_ROOT}/docker/docker-entrypoint.sh"
ARCHIVE_DIR="${REPO_ROOT}/docker/artifacts"

IMAGE_TAG="${IMAGE_TAG:-clawbrowser:local}"
BUILD_SOURCE="${BUILD_SOURCE:-}"
TMP_DIR=""
DETECTED_PLATFORM="${DOCKER_PLATFORM:-}"

log() {
  printf '[clawbrowser-docker-build] %s\n' "$*"
}

die() {
  printf '[clawbrowser-docker-build] ERROR: %s\n' "$*" >&2
  exit 1
}

usage() {
  cat <<'EOF'
Usage:
  scripts/build_docker_image.sh [--build-source PATH] [--image-tag NAME]

Build a Docker image that packages a Linux Chromium build output and launches it
inside a virtual X display via Xvfb.

Options:
  --build-source PATH  Path to a Linux build output directory, a Chromium src
                       tree that contains out/*/chrome, or a .tar.gz runtime
                       bundle such as clawbrowser-prod-linux-arm64.tar.gz
  --image-tag NAME     Docker image tag. Default: clawbrowser:local
  --help               Show this help

Examples:
  bash scripts/build_docker_image.sh \
    --build-source /path/to/chromium/src/out/Default \
    --image-tag clawbrowser:headed

  bash scripts/build_docker_image.sh \
    --build-source /path/to/chromium/src \
    --image-tag clawbrowser:headed

  bash scripts/build_docker_image.sh \
    --build-source ./docker/artifacts/clawbrowser-prod-linux-arm64.tar.gz \
    --image-tag clawbrowser:headed
EOF
}

require_command() {
  local name="$1"
  if ! command -v "${name}" >/dev/null 2>&1; then
    die "Required command not found: ${name}"
  fi
}

is_archive_path() {
  local source="$1"
  case "${source}" in
    *.tar.gz|*.tgz|*.tar)
      return 0
      ;;
  esac
  return 1
}

is_valid_build_dir() {
  local build_dir="$1"
  local path
  local -a required_paths=(
    "chrome"
    "chrome_crashpad_handler"
    "icudtl.dat"
    "locales"
    "resources.pak"
  )

  [[ -d "${build_dir}" ]] || return 1

  for path in "${required_paths[@]}"; do
    [[ -e "${build_dir}/${path}" ]] || return 1
  done

  return 0
}

validate_build_dir() {
  local build_dir="$1"
  local path
  local -a required_paths=(
    "chrome"
    "chrome_crashpad_handler"
    "icudtl.dat"
    "locales"
    "resources.pak"
  )

  for path in "${required_paths[@]}"; do
    if [[ ! -e "${build_dir}/${path}" ]]; then
      die "Build output is missing ${path}: ${build_dir}/${path}"
    fi
  done
}

resolve_build_dir() {
  local source="$1"
  local candidate
  local -a candidates=(
    "${source}"
    "${source}/out/CBFast"
    "${source}/out/Default"
    "${source}/out/Release"
  )

  for candidate in "${candidates[@]}"; do
    if is_valid_build_dir "${candidate}"; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  while IFS= read -r -d '' candidate; do
    candidate="$(dirname "${candidate}")"
    if is_valid_build_dir "${candidate}"; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done < <(find "${source}" -maxdepth 4 -type f -name chrome -print0 2>/dev/null)

  if [[ -d "${source}/Chromium.app" || -d "${source}/out/CBFast/Chromium.app" ]]; then
    die "Detected a macOS Chromium app bundle under ${source}. The Docker image expects a Linux build output that contains a chrome binary."
  fi

  die "Could not find a Linux Chromium build output under ${source}. Expected a directory containing an executable chrome binary."
}

detect_repo_default_source() {
  local host_arch
  local -a candidates=()

  host_arch="$(uname -m)"
  case "${host_arch}" in
    arm64|aarch64)
      candidates+=(
        "${ARCHIVE_DIR}/clawbrowser-prod-linux-arm64.tar.gz"
        "${ARCHIVE_DIR}/clawbrowser-prod-linux-aarch64.tar.gz"
        "${ARCHIVE_DIR}/clawbrowser-prod-linux-x64.tar.gz"
        "${REPO_ROOT}/clawbrowser-prod-linux-arm64.tar.gz"
        "${REPO_ROOT}/clawbrowser-prod-linux-aarch64.tar.gz"
        "${REPO_ROOT}/clawbrowser-prod-linux-x64.tar.gz"
      )
      ;;
    x86_64|amd64)
      candidates+=(
        "${ARCHIVE_DIR}/clawbrowser-prod-linux-x64.tar.gz"
        "${ARCHIVE_DIR}/clawbrowser-prod-linux-amd64.tar.gz"
        "${ARCHIVE_DIR}/clawbrowser-prod-linux-arm64.tar.gz"
        "${REPO_ROOT}/clawbrowser-prod-linux-x64.tar.gz"
        "${REPO_ROOT}/clawbrowser-prod-linux-amd64.tar.gz"
        "${REPO_ROOT}/clawbrowser-prod-linux-arm64.tar.gz"
      )
      ;;
  esac
  candidates+=(
    "${ARCHIVE_DIR}/clawbrowser-prod-linux-arm64.tar.gz"
    "${ARCHIVE_DIR}/clawbrowser-prod-linux-x64.tar.gz"
    "${REPO_ROOT}/clawbrowser-prod-linux-arm64.tar.gz"
    "${REPO_ROOT}/clawbrowser-prod-linux-x64.tar.gz"
  )

  for source in "${candidates[@]}"; do
    if [[ -f "${source}" ]]; then
      printf '%s\n' "${source}"
      return 0
    fi
  done

  die "--build-source is required because no repo-local linux runtime archive was found."
}

prepare_source_root() {
  local source="$1"
  local extract_root

  if is_archive_path "${source}"; then
    extract_root="${TMP_DIR}/extracted"
    mkdir -p "${extract_root}"
    case "${source}" in
      *.tar.gz|*.tgz)
        tar -xzf "${source}" -C "${extract_root}"
        ;;
      *.tar)
        tar -xf "${source}" -C "${extract_root}"
        ;;
    esac
    printf '%s\n' "${extract_root}"
    return 0
  fi

  printf '%s\n' "${source}"
}

detect_platform() {
  local build_dir="$1"
  local binary_path="$build_dir/chrome.real"
  local file_output

  [[ -n "${DETECTED_PLATFORM}" ]] && return 0

  if [[ ! -f "${binary_path}" ]]; then
    binary_path="$build_dir/chrome"
  fi

  file_output="$(file "${binary_path}")"
  case "${file_output}" in
    *"ARM aarch64"*)
      DETECTED_PLATFORM="linux/arm64"
      ;;
    *"x86-64"*)
      DETECTED_PLATFORM="linux/amd64"
      ;;
  esac
}

parse_args() {
  while (($# > 0)); do
    case "$1" in
      --build-source)
        BUILD_SOURCE="$2"
        shift 2
        ;;
      --image-tag)
        IMAGE_TAG="$2"
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

main() {
  local build_dir
  local source_root
  local build_source_abs

  parse_args "$@"

  if [[ -z "${BUILD_SOURCE}" ]]; then
    BUILD_SOURCE="$(detect_repo_default_source)"
  fi

  [[ -f "${DOCKERFILE_PATH}" ]] || die "Dockerfile not found: ${DOCKERFILE_PATH}"
  [[ -f "${ENTRYPOINT_PATH}" ]] || die "Entrypoint script not found: ${ENTRYPOINT_PATH}"

  require_command docker
  require_command file
  require_command mktemp
  require_command rsync
  require_command tar

  if is_archive_path "${BUILD_SOURCE}"; then
    build_source_abs="$(cd "$(dirname "${BUILD_SOURCE}")" && pwd)/$(basename "${BUILD_SOURCE}")"
  else
    build_source_abs="$(cd "${BUILD_SOURCE}" && pwd)"
  fi

  TMP_DIR="$(mktemp -d)"
  trap 'if [[ -n "${TMP_DIR}" ]]; then rm -rf "${TMP_DIR}"; fi' EXIT

  source_root="$(prepare_source_root "${build_source_abs}")"
  build_dir="$(resolve_build_dir "${source_root}")"
  validate_build_dir "${build_dir}"
  detect_platform "${build_dir}"

  mkdir -p "${TMP_DIR}/clawbrowser-dist"
  cp "${ENTRYPOINT_PATH}" "${TMP_DIR}/docker-entrypoint.sh"
  rsync -a "${build_dir}/" "${TMP_DIR}/clawbrowser-dist/"

  log "Building ${IMAGE_TAG} from ${build_dir}"
  docker build \
    ${DETECTED_PLATFORM:+"--platform=${DETECTED_PLATFORM}"} \
    --tag "${IMAGE_TAG}" \
    --file "${DOCKERFILE_PATH}" \
    "${TMP_DIR}"

  log "Built image ${IMAGE_TAG}"
  if [[ -n "${DETECTED_PLATFORM}" ]]; then
    log "Detected image platform ${DETECTED_PLATFORM}"
  fi
  log "Example run:"
  log "  docker run --rm -it -p 9222:9222 ${IMAGE_TAG} --remote-debugging-address=0.0.0.0 --remote-debugging-port=9222"
}

main "$@"
