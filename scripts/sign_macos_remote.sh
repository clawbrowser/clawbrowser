#!/usr/bin/env bash
# Signs and notarizes a macOS Clawbrowser.app with a Developer ID key that lives
# on another machine, using rcodesign remote signing:
#
#   this machine (initiator)                      signer (over SSH)
#   rcodesign sign --remote-signer ...  --sjs-->  rcodesign remote-sign <sjs>
#   (has the app, never the key)                  (has the .p12, never the app)
#
# The session join string (sjs) is a one-time token printed by the initiator;
# this script reads it and hands it to the signer over SSH. Signing traffic is
# relayed (end-to-end encrypted) through rcodesign's default relay server.
#
# Run on macOS or Linux. Not on Windows: extracting the bundle onto NTFS loses
# the framework's Versions/Current symlinks and exec bits and breaks the app.
set -euo pipefail

RCODESIGN="${RCODESIGN:-rcodesign}"
REMOTE_RCODESIGN="${REMOTE_RCODESIGN:-rcodesign}"
SJS_TIMEOUT_SECONDS="${SJS_TIMEOUT_SECONDS:-60}"

INPUT=""
OUTPUT=""
SIGNER=""
SIGNER_P12=""
SIGNER_P12_PASSWORD_FILE=""
CERT_PEM=""
ENTITLEMENTS_DIR=""
API_KEY_FILE=""
NOTARIZE=1
SSH_OPTS=()
WORK_DIR=""
SIGN_SETTINGS=()

log() {
  printf '[sign-macos-remote] %s\n' "$*"
}

die() {
  printf '[sign-macos-remote] ERROR: %s\n' "$*" >&2
  exit 1
}

usage() {
  cat <<'EOF'
Usage:
  scripts/sign_macos_remote.sh --input <Clawbrowser.app|clawbrowser-macos-arm64.tar.gz>
      --signer <user@host> --signer-p12 <remote .p12> --signer-p12-password-file <remote file>
      --entitlements-dir <chromium src/chrome/app>
      (--api-key-file <App Store Connect key .json> | --no-notarize)
      [--cert-pem <Developer ID cert .pem>] [--output <signed .tar.gz>] [--ssh-opt <opt>]...

Options:
  --input                     Unsigned (or ad-hoc signed) Clawbrowser.app, or the
                              tar.gz produced by build_remote_prod_artifacts.sh.
  --output                    Signed archive to write. Default: <input>-signed.tar.gz
  --signer                    SSH destination of the machine holding the key.
  --signer-p12                Path of the Developer ID Application .p12 on the signer.
  --signer-p12-password-file  Path of a file with the .p12 password on the signer.
  --entitlements-dir          Directory with app-entitlements.plist,
                              helper-renderer-entitlements.plist and
                              helper-gpu-entitlements.plist (chromium/src/chrome/app).
  --cert-pem                  Local PEM of the Developer ID certificate (public part).
                              Default: extracted from the signer's .p12 with openssl.
  --api-key-file              App Store Connect API key JSON, created with
                              `rcodesign encode-app-store-connect-api-key`.
  --no-notarize               Sign only; skip notarization and stapling.
  --ssh-opt                   Extra ssh option, repeatable (e.g. --ssh-opt -i --ssh-opt ~/.ssh/key).

Environment:
  RCODESIGN, REMOTE_RCODESIGN  rcodesign binary locally / on the signer.
  SJS_TIMEOUT_SECONDS          How long to wait for the session join string.
EOF
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --input) INPUT="$2"; shift 2 ;;
      --output) OUTPUT="$2"; shift 2 ;;
      --signer) SIGNER="$2"; shift 2 ;;
      --signer-p12) SIGNER_P12="$2"; shift 2 ;;
      --signer-p12-password-file) SIGNER_P12_PASSWORD_FILE="$2"; shift 2 ;;
      --cert-pem) CERT_PEM="$2"; shift 2 ;;
      --entitlements-dir) ENTITLEMENTS_DIR="$2"; shift 2 ;;
      --api-key-file) API_KEY_FILE="$2"; shift 2 ;;
      --no-notarize) NOTARIZE=0; shift ;;
      --ssh-opt) SSH_OPTS+=("$2"); shift 2 ;;
      -h|--help) usage; exit 0 ;;
      *) usage >&2; die "unknown argument: $1" ;;
    esac
  done

  [[ -n "${INPUT}" ]] || die "--input is required"
  [[ -n "${SIGNER}" ]] || die "--signer is required"
  [[ -n "${SIGNER_P12}" ]] || die "--signer-p12 is required"
  [[ -n "${SIGNER_P12_PASSWORD_FILE}" ]] || die "--signer-p12-password-file is required"
  [[ -n "${ENTITLEMENTS_DIR}" ]] || die "--entitlements-dir is required"
  if [[ "${NOTARIZE}" == "1" ]]; then
    [[ -n "${API_KEY_FILE}" ]] || die "--api-key-file is required (or pass --no-notarize)"
    [[ -f "${API_KEY_FILE}" ]] || die "API key file not found: ${API_KEY_FILE}"
  fi
}

# Runs a command on the signer; each argument is quoted for the remote shell.
remote() {
  local quoted=""
  printf -v quoted '%q ' "$@"
  # ${arr[@]+...}: empty arrays are "unbound" under set -u in macOS bash 3.2.
  ssh ${SSH_OPTS[@]+"${SSH_OPTS[@]}"} -- "${SIGNER}" "${quoted}"
}

preflight() {
  case "$(uname -s)" in
    Darwin|Linux) ;;
    *) die "run this on macOS or Linux (see header comment), not $(uname -s)" ;;
  esac
  command -v "${RCODESIGN}" >/dev/null || die "rcodesign not found locally: ${RCODESIGN}"

  local name
  for name in app-entitlements.plist helper-renderer-entitlements.plist helper-gpu-entitlements.plist; do
    [[ -f "${ENTITLEMENTS_DIR}/${name}" ]] || die "missing ${ENTITLEMENTS_DIR}/${name}"
  done

  log "checking signer ${SIGNER}"
  remote "${REMOTE_RCODESIGN}" --version >/dev/null || \
    die "rcodesign not runnable on signer: ${REMOTE_RCODESIGN}"
  remote test -f "${SIGNER_P12}" || die "signer .p12 not found: ${SIGNER_P12}"
  remote test -f "${SIGNER_P12_PASSWORD_FILE}" || \
    die "signer password file not found: ${SIGNER_P12_PASSWORD_FILE}"
}

# Writes the public Developer ID certificate to $1. The private key stays on
# the signer; only the certificate crosses the wire.
fetch_cert_pem() {
  local dest="$1"

  if [[ -n "${CERT_PEM}" ]]; then
    cp "${CERT_PEM}" "${dest}"
  else
    log "extracting certificate from signer .p12"
    remote sh -c 'openssl pkcs12 -in "$1" -nokeys -clcerts -passin "file:$2" | openssl x509' \
      _ "${SIGNER_P12}" "${SIGNER_P12_PASSWORD_FILE}" >"${dest}" || \
      die "could not extract the certificate; export it and pass --cert-pem"
  fi
  grep -q 'BEGIN CERTIFICATE' "${dest}" || die "no certificate in ${dest}"
}

# Prints Mach-O paths under the app root, relative to it (detected by magic).
macho_executables() {
  local app="$1" file magic

  (cd "${app}" && find . -type f -print) | sed 's|^\./||' | while IFS= read -r file; do
    magic="$(od -An -tx1 -N4 "${app}/${file}" | tr -d ' \n')"
    case "${magic}" in
      cffaedfe|cefaedfe|feedfacf|feedface|cafebabe) printf '%s\n' "${file}" ;;
    esac
  done
}

# Builds the rcodesign settings that mirror chrome/installer/mac/signing/parts.py:
# hardened runtime everywhere, app entitlements on the outer app, JIT
# entitlements on the renderer and GPU helpers. rcodesign does not inherit
# entitlements or code signature flags into nested code, so each nested bundle
# and loose Mach-O gets a path-scoped setting (path relative to the app root).
# Fills SIGN_SETTINGS (no namerefs: macOS ships bash 3.2).
build_sign_settings() {
  local app="$1"
  local bundle file

  SIGN_SETTINGS=(
    --code-signature-flags runtime
    --entitlements-xml-path ent/app-entitlements.plist
  )

  while IFS= read -r bundle; do
    SIGN_SETTINGS+=(--code-signature-flags "${bundle}:runtime")
    case "${bundle}" in
      *" Helper (Renderer).app")
        SIGN_SETTINGS+=(--entitlements-xml-path "${bundle}:ent/helper-renderer-entitlements.plist") ;;
      *" Helper (GPU).app")
        SIGN_SETTINGS+=(--entitlements-xml-path "${bundle}:ent/helper-gpu-entitlements.plist") ;;
    esac
  done < <(cd "${app}" && find . -mindepth 2 -type d -name '*.app' -print | sed 's|^\./||')

  # Loose executables (crashpad handler, app_mode_loader, dylibs, ...). Bundle
  # main executables under Contents/MacOS take their bundle's settings.
  while IFS= read -r file; do
    case "${file}" in
      Contents/MacOS/*|*/Contents/MacOS/*) ;;
      *) SIGN_SETTINGS+=(--code-signature-flags "${file}:runtime") ;;
    esac
  done < <(macho_executables "${app}")
}

# Starts the initiator, waits for its one-time session join string, and has
# the signer join the session over SSH.
remote_sign() {
  local log_file="$1"
  shift
  local pid sjs="" waited=0

  "${RCODESIGN}" sign --remote-signer --remote-public-key-pem-file cert.pem "$@" \
    >"${log_file}" 2>&1 &
  pid=$!

  while [[ -z "${sjs}" ]]; do
    sjs="$(sed -n 's/^[[:space:]]*rcodesign remote-sign \([^[:space:]]*\).*$/\1/p' "${log_file}" | head -n1)"
    [[ -n "${sjs}" ]] && break
    if ! kill -0 "${pid}" 2>/dev/null; then
      cat "${log_file}" >&2
      die "rcodesign exited before printing a session join string"
    fi
    if (( waited >= SJS_TIMEOUT_SECONDS * 2 )); then
      kill "${pid}" 2>/dev/null || true
      cat "${log_file}" >&2
      die "timed out waiting for the session join string"
    fi
    sleep 0.5
    waited=$((waited + 1))
  done

  log "session created; signer ${SIGNER} is joining"
  if ! remote "${REMOTE_RCODESIGN}" remote-sign \
      --p12-file "${SIGNER_P12}" \
      --p12-password-file "${SIGNER_P12_PASSWORD_FILE}" \
      "${sjs}"; then
    kill "${pid}" 2>/dev/null || true
    cat "${log_file}" >&2
    die "signer failed to sign"
  fi

  if ! wait "${pid}"; then
    cat "${log_file}" >&2
    die "rcodesign sign failed"
  fi
}

# Fails if the path-scoped settings did not land (e.g. rcodesign resolved the
# framework through Versions/Current instead of the real Versions/<n> path).
verify_signature() {
  local app="$1" helper exe info

  info="$("${RCODESIGN}" print-signature-info "${app}/Contents/MacOS/"*)"
  grep -qi 'runtime' <<<"${info}" || die "outer app executable lacks hardened runtime"

  helper="$(find "${app}" -type d -name '* Helper (Renderer).app' -print -quit)"
  [[ -n "${helper}" ]] || die "renderer helper not found in ${app}"
  exe="$(find "${helper}/Contents/MacOS" -type f -print -quit)"
  info="$("${RCODESIGN}" print-signature-info "${exe}")"
  grep -qi 'runtime' <<<"${info}" || die "renderer helper lacks hardened runtime"
  grep -q 'com.apple.security.cs.allow-jit' <<<"${info}" || \
    die "renderer helper lacks the allow-jit entitlement"

  if [[ "$(uname -s)" == "Darwin" ]]; then
    codesign --verify --deep --strict --verbose=2 "${app}"
  fi
}

main() {
  parse_args "$@"
  preflight

  local work app_name src_app
  WORK_DIR="$(mktemp -d)"
  work="${WORK_DIR}"
  trap 'rm -rf "${WORK_DIR}"' EXIT

  mkdir -p "${work}/in" "${work}/out" "${work}/ent"
  case "${INPUT}" in
    *.tar.gz|*.tgz)
      tar -xzf "${INPUT}" -C "${work}/in"
      src_app="$(find "${work}/in" -mindepth 1 -maxdepth 1 -type d -name '*.app' -print -quit)"
      [[ -n "${src_app}" ]] || die "no .app at the top of ${INPUT}"
      ;;
    *.app|*.app/)
      [[ -d "${INPUT}" ]] || die "not a directory: ${INPUT}"
      src_app="${work}/in/$(basename "${INPUT%/}")"
      cp -a "${INPUT%/}" "${src_app}"
      ;;
    *) die "--input must be a .app or .tar.gz" ;;
  esac
  app_name="$(basename "${src_app}")"
  : "${OUTPUT:=$(cd "$(dirname "${INPUT}")" && pwd)/${app_name%.app}-signed.tar.gz}"

  cp "${ENTITLEMENTS_DIR}"/{app,helper-renderer,helper-gpu}-entitlements.plist "${work}/ent/"
  fetch_cert_pem "${work}/cert.pem"

  build_sign_settings "${src_app}"

  # Relative paths throughout: rcodesign splits scoped values at the first colon.
  log "signing ${app_name}"
  (
    cd "${work}"
    remote_sign "${work}/sign.log" "${SIGN_SETTINGS[@]}" "in/${app_name}" "out/${app_name}"
  )
  verify_signature "${work}/out/${app_name}"

  if [[ "${NOTARIZE}" == "1" ]]; then
    log "notarizing and stapling (this can take several minutes)"
    "${RCODESIGN}" notary-submit --api-key-file "${API_KEY_FILE}" --staple "${work}/out/${app_name}"
    if [[ "$(uname -s)" == "Darwin" ]]; then
      spctl --assess --type execute --verbose=2 "${work}/out/${app_name}"
    fi
  fi

  COPYFILE_DISABLE=1 tar -czf "${OUTPUT}" -C "${work}/out" "${app_name}"
  log "wrote ${OUTPUT}"
}

main "$@"
