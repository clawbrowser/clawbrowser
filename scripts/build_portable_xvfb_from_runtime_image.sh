#!/usr/bin/env bash
# Migrated from clawctl_internal at 1a89d7777654c6802d3b38eb0d162e6d7d02b2a9.
# Keep the release packager alongside the workflow and Dockerfile that use it.
set -euo pipefail
IFS=$'\n\t'

IMAGE_REF="${IMAGE_REF:-}"
VERSION="${VERSION:-}"
OUT_DIR="${OUT_DIR:-dist}"
PLATFORM="${PLATFORM:-}"
STRIP_BINARIES="${STRIP_BINARIES:-0}"

tmp_root=""
container_id=""

log() {
  printf '[portable-build] %s\n' "$*"
}

die() {
  printf '[portable-build] ERROR: %s\n' "$*" >&2
  exit 1
}

usage() {
  cat <<'EOF'
Builds v0 portable Xvfb runtime artifacts from a Docker runtime image.

Required env vars:
  IMAGE_REF   Docker image reference containing runtime files
  VERSION     Runtime version to write into manifest.json

Optional env vars:
  OUT_DIR         Output directory (default: dist)
  PLATFORM        Docker platform: linux/amd64 or linux/arm64
                  Default: linux/amd64 on x86_64 hosts, linux/arm64 on arm64 hosts
  STRIP_BINARIES  Set to 1 to strip ELF binaries/libs (default: 0)

Example:
  IMAGE_REF=docker.io/clawbrowser/clawbrowser:latest \
  VERSION=1.2.3 \
  OUT_DIR=dist \
  PLATFORM=linux/amd64 \
  ./scripts/build_portable_xvfb_from_runtime_image.sh
EOF
}

cleanup() {
  local exit_code=$?
  trap - EXIT INT TERM
  if [[ -n "${container_id}" ]]; then
    docker rm -f "${container_id}" >/dev/null 2>&1 || true
  fi
  if [[ -n "${tmp_root}" && -d "${tmp_root}" ]]; then
    rm -rf "${tmp_root}" >/dev/null 2>&1 || true
  fi
  exit "${exit_code}"
}

require_command() {
  local command_name="$1"
  if ! command -v "${command_name}" >/dev/null 2>&1; then
    die "Required command not found: ${command_name}"
  fi
}

default_platform() {
  case "$(uname -m)" in
    x86_64|amd64) printf '%s\n' linux/amd64 ;;
    arm64|aarch64) printf '%s\n' linux/arm64 ;;
    *) die "Unsupported host architecture for default PLATFORM: $(uname -m). Set PLATFORM explicitly." ;;
  esac
}

platform_arch() {
  case "${PLATFORM}" in
    linux/amd64) printf '%s\n' amd64 ;;
    linux/arm64|linux/arm64/v8) printf '%s\n' arm64 ;;
    *) return 1 ;;
  esac
}

platform_debian_arch_dir() {
  case "$(platform_arch)" in
    amd64) printf '%s\n' x86_64-linux-gnu ;;
    arm64) printf '%s\n' aarch64-linux-gnu ;;
    *) return 1 ;;
  esac
}

platform_dir() {
  printf 'linux-%s-glibc\n' "$(platform_arch)"
}

artifact_basename() {
  printf 'clawbrowser-portable-linux-%s-glibc\n' "$(platform_arch)"
}

assert_inputs() {
  if [[ -z "${IMAGE_REF}" ]]; then
    usage
    die "Missing required IMAGE_REF."
  fi
  if [[ -z "${VERSION}" ]]; then
    usage
    die "Missing required VERSION."
  fi
  if [[ -z "${PLATFORM}" ]]; then
    PLATFORM="$(default_platform)"
  fi
  if ! platform_arch >/dev/null 2>&1; then
    die "Unsupported PLATFORM=${PLATFORM}. Portable packaging supports linux/amd64 and linux/arm64."
  fi
  case "${STRIP_BINARIES}" in
    0|1) ;;
    *) die "STRIP_BINARIES must be 0 or 1 (got: ${STRIP_BINARIES})." ;;
  esac
}

copy_dir_required() {
  local src_path="$1"
  local dst_path="$2"
  local src_basename tmp_copy_dir

  src_basename="$(basename "${src_path}")"
  tmp_copy_dir="$(mktemp -d "${tmp_root}/cpdir.XXXXXX")"
  if ! docker cp "${container_id}:${src_path}" "${tmp_copy_dir}/"; then
    die "Failed to copy required directory from image: ${src_path}"
  fi
  if [[ ! -e "${tmp_copy_dir}/${src_basename}" ]]; then
    die "Copied directory missing after docker cp: ${src_path}"
  fi

  mkdir -p "$(dirname "${dst_path}")"
  rm -rf "${dst_path}"
  mv "${tmp_copy_dir}/${src_basename}" "${dst_path}"
  rm -rf "${tmp_copy_dir}"
}

copy_dir_optional() {
  local src_path="$1"
  local dst_path="$2"
  local src_basename tmp_copy_dir

  src_basename="$(basename "${src_path}")"
  tmp_copy_dir="$(mktemp -d "${tmp_root}/cpdir.XXXXXX")"
  if ! docker cp "${container_id}:${src_path}" "${tmp_copy_dir}/" >/dev/null 2>&1; then
    rm -rf "${tmp_copy_dir}"
    return 1
  fi
  if [[ ! -e "${tmp_copy_dir}/${src_basename}" ]]; then
    rm -rf "${tmp_copy_dir}"
    return 1
  fi

  mkdir -p "$(dirname "${dst_path}")"
  rm -rf "${dst_path}"
  mv "${tmp_copy_dir}/${src_basename}" "${dst_path}"
  rm -rf "${tmp_copy_dir}"
  log "Copied optional directory: ${src_path}"
}

copy_file_required() {
  local src_path="$1"
  local dst_path="$2"

  mkdir -p "$(dirname "${dst_path}")"
  if ! docker cp "${container_id}:${src_path}" "${dst_path}"; then
    die "Failed to copy required file from image: ${src_path}"
  fi
}

copy_file_optional() {
  local src_path="$1"
  local dst_path="$2"
  mkdir -p "$(dirname "${dst_path}")"
  if docker cp "${container_id}:${src_path}" "${dst_path}" >/dev/null 2>&1; then
    log "Copied optional file: ${src_path}"
    return 0
  fi
  return 1
}

normalize_portable_runtime_links() {
  local runtime_dir="$1"
  local link_path link_target link_name source_path

  # The runtime image keeps rgb.txt as a symlink to /etc/X11/rgb.txt. The
  # portable archive needs a real file here, or the extractor will reject the
  # absolute symlink.
  rm -f "${runtime_dir}/share/X11/rgb.txt"
  copy_file_required "/etc/X11/rgb.txt" "${runtime_dir}/share/X11/rgb.txt"

  # Fontconfig's conf.d entries come out of the runtime image as symlinks to
  # conf.avail. The portable extractor rejects links with ".." or absolute
  # paths, so materialize those configs as plain files.
  if [[ -d "${runtime_dir}/etc/fonts/conf.d" ]]; then
    while IFS= read -r -d '' link_path; do
      link_target="$(readlink "${link_path}")"
      case "${link_target}" in
        /usr/share/fontconfig/conf.avail/*)
          link_name="$(basename "${link_target}")"
          source_path="${runtime_dir}/share/fontconfig/conf.avail/${link_name}"
          rm -f "${link_path}"
          cp -f "${source_path}" "${link_path}"
          ;;
        ../conf.avail/*)
          link_name="$(basename "${link_target}")"
          source_path="${runtime_dir}/etc/fonts/conf.avail/${link_name}"
          rm -f "${link_path}"
          cp -f "${source_path}" "${link_path}"
          ;;
      esac
    done < <(find "${runtime_dir}/etc/fonts/conf.d" -maxdepth 1 -type l -print0)
  fi
}

prune_runtime_tree() {
  local runtime_dir="$1"
  local path

  while IFS= read -r path; do
    rm -rf "${path}"
  done < <(find "${runtime_dir}" -type d \( -name '__pycache__' -o -name '.cache' \))

  find "${runtime_dir}" -type f \
    \( -name '*.a' -o -name '*.la' -o -name '*.o' -o -name '*.pyc' -o -name '*.pyo' -o -name '*.tmp' -o -name '._*' -o -name '.DS_Store' \) \
    -delete

  if [[ -d "${runtime_dir}/lib" ]]; then
    while IFS= read -r path; do
      rm -rf "${path}"
    done < <(find "${runtime_dir}/lib" -type d \
      \( -name 'cmake' -o -name 'pkgconfig' -o -name 'include' -o -name 'gtk-doc' -o -name 'man' -o -name 'doc' \))
  fi
}

strip_runtime_tree() {
  local runtime_dir="$1"
  local candidate
  local file_info

  require_command strip
  require_command file

  log "Stripping ELF payloads under ${runtime_dir}"
  while IFS= read -r -d '' candidate; do
    file_info="$(file -b "${candidate}" 2>/dev/null || true)"
    if [[ "${file_info}" != *ELF* ]]; then
      continue
    fi
    strip --strip-unneeded "${candidate}" >/dev/null 2>&1 || true
  done < <(find "${runtime_dir}" -type f \
    \( -perm -u+x -o -name '*.so' -o -name '*.so.*' \) -print0)
}

sha256_hex() {
  local file_path="$1"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "${file_path}" | awk '{print $1}'
    return
  fi
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "${file_path}" | awk '{print $1}'
    return
  fi
  die "Could not find sha256sum or shasum."
}

write_manifest() {
  local manifest_path="$1"
  local image_digest="$2"
  local portable_platform

  portable_platform="$(platform_dir)"
  python3 - "${manifest_path}" "${VERSION}" "${IMAGE_REF}" "${image_digest}" "${portable_platform}" <<'PY'
import json
import pathlib
import sys

manifest_path = pathlib.Path(sys.argv[1])
version = sys.argv[2]
image_ref = sys.argv[3]
image_digest = sys.argv[4]
platform = sys.argv[5]

payload = {
    "kind": "clawbrowser-portable",
    "version": version,
    "platform": platform,
    "headful": True,
    "headless_allowed": False,
    "source": {
        "type": "runtime-image",
        "image": image_ref,
        "digest": image_digest,
    },
    "entrypoints": {
        "browser": "clawbrowser/clawbrowser.real",
        "xvfb": "bin/Xvfb",
        "xkbcomp": "bin/xkbcomp",
        "dbus_run_session": "bin/dbus-run-session",
    },
    "default_screen": "1920x1080x24",
    "default_flags": [
        "--no-first-run",
        "--no-default-browser-check",
        "--disable-dev-shm-usage",
        "--no-sandbox",
    ],
}

manifest_path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
PY
}

main() {
  trap cleanup EXIT INT TERM

  require_command docker
  require_command tar
  require_command mktemp
  require_command python3

  assert_inputs

  mkdir -p "${OUT_DIR}"
  tmp_root="$(mktemp -d)"

  log "Creating container from ${IMAGE_REF} (${PLATFORM})"
  if ! container_id="$(docker create --platform "${PLATFORM}" "${IMAGE_REF}")"; then
    die "docker create failed for IMAGE_REF=${IMAGE_REF} and PLATFORM=${PLATFORM}."
  fi

  local stage_root runtime_dir manifest_path out_tar out_sha out_manifest digest image_digest
  local runtime_platform_dir portable_artifact_basename tar_name sha_name manifest_asset_name debian_arch_dir
  stage_root="${tmp_root}/stage"
  runtime_platform_dir="$(platform_dir)"
  portable_artifact_basename="$(artifact_basename)"
  tar_name="${portable_artifact_basename}.tar.gz"
  sha_name="${tar_name}.sha256"
  manifest_asset_name="${portable_artifact_basename}.manifest.json"
  debian_arch_dir="$(platform_debian_arch_dir)"
  runtime_dir="${stage_root}/${runtime_platform_dir}"
  manifest_path="${runtime_dir}/manifest.json"
  out_tar="${OUT_DIR}/${tar_name}"
  out_sha="${OUT_DIR}/${sha_name}"
  out_manifest="${OUT_DIR}/${manifest_asset_name}"
  image_digest="$(docker image inspect --format '{{.Id}}' "${IMAGE_REF}" 2>/dev/null || true)"

  mkdir -p "${runtime_dir}/bin" "${runtime_dir}/lib" "${runtime_dir}/share" "${runtime_dir}/etc"

  log "Copying runtime payload from /opt/clawbrowser"
  copy_dir_required "/opt/clawbrowser" "${runtime_dir}/clawbrowser"

  log "Copying Xvfb/xkbcomp/dbus binaries"
  copy_file_required "/usr/bin/Xvfb" "${runtime_dir}/bin/Xvfb"
  copy_file_required "/usr/bin/xkbcomp" "${runtime_dir}/bin/xkbcomp"
  copy_file_optional "/usr/bin/dbus-run-session" "${runtime_dir}/bin/dbus-run-session" || true
  copy_file_optional "/usr/bin/dbus-daemon" "${runtime_dir}/bin/dbus-daemon" || true
  copy_file_optional "/usr/bin/dbus-launch" "${runtime_dir}/bin/dbus-launch" || true
  copy_file_optional "/usr/bin/dbus-update-activation-environment" "${runtime_dir}/bin/dbus-update-activation-environment" || true

  log "Copying X11/fontconfig assets"
  copy_dir_required "/usr/share/X11" "${runtime_dir}/share/X11"
  copy_dir_required "/usr/share/fonts" "${runtime_dir}/share/fonts"
  copy_dir_required "/etc/fonts" "${runtime_dir}/etc/fonts"
  copy_dir_optional "/usr/share/fontconfig" "${runtime_dir}/share/fontconfig" || true
  copy_dir_optional "/usr/share/glvnd" "${runtime_dir}/share/glvnd" || true
  copy_dir_optional "/usr/share/dbus-1" "${runtime_dir}/share/dbus-1" || true
  copy_dir_optional "/etc/dbus-1" "${runtime_dir}/etc/dbus-1" || true
  normalize_portable_runtime_links "${runtime_dir}"

  log "Copying fat library directories"
  copy_dir_required "/lib/${debian_arch_dir}" "${runtime_dir}/lib/${debian_arch_dir}"
  copy_dir_required "/usr/lib/${debian_arch_dir}" "${runtime_dir}/lib/usr-${debian_arch_dir}"

  log "Pruning obvious non-runtime files"
  prune_runtime_tree "${runtime_dir}"

  if [[ "${STRIP_BINARIES}" == "1" ]]; then
    strip_runtime_tree "${runtime_dir}"
  fi

  write_manifest "${manifest_path}" "${image_digest}"

  log "Packing ${tar_name}"
  rm -f "${out_tar}" "${out_sha}" "${out_manifest}"
  COPYFILE_DISABLE=1 tar -C "${stage_root}" -czf "${out_tar}" "${runtime_platform_dir}"

  digest="$(sha256_hex "${out_tar}")"
  printf '%s  %s\n' "${digest}" "${tar_name}" >"${out_sha}"
  cp "${manifest_path}" "${out_manifest}"

  log "Wrote ${out_tar}"
  log "Wrote ${out_sha}"
  log "Wrote ${out_manifest}"
}

main "$@"
