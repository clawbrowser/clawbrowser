#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
README_FILE="${ROOT_DIR}/README.md"
INSTALL_FILE="${ROOT_DIR}/INSTALL.md"
INSTALL_SCRIPT="${ROOT_DIR}/scripts/install.sh"

fail() {
  printf 'FAIL: %s\n' "$*" >&2
  exit 1
}

assert_file_contains() {
  local file="$1"
  local needle="$2"
  if ! grep -Fq -- "$needle" "$file"; then
    fail "expected ${file} to contain: ${needle}"
  fi
}

run_test() {
  local name="$1"
  shift
  printf 'ok - %s\n' "${name}"
  "$@"
}

test_portable_is_default_linux_path() {
  assert_file_contains "${README_FILE}" "### Linux Server / Container / No Root"
  assert_file_contains "${INSTALL_FILE}" "| Linux VPS/server/no display | portable | No | No |"
  assert_file_contains "${INSTALL_FILE}" "| Restricted container/no root | portable | No | No |"
}

test_restricted_container_message_points_to_portable_first() {
  assert_file_contains "${INSTALL_SCRIPT}" "The default Linux path is the portable runtime (full headful Clawbrowser under Xvfb), ensured by clawctl install as a separate release artifact"
  assert_file_contains "${INSTALL_SCRIPT}" "If your operator intentionally uses Docker backend sidecar mode, the host/operator must provision it"
}

test_docker_is_optional_operator_managed() {
  assert_file_contains "${README_FILE}" "**Docker backend** - optional, operator-managed infrastructure only."
  assert_file_contains "${INSTALL_FILE}" "## Optional Docker Backend"
  assert_file_contains "${INSTALL_SCRIPT}" "Optional Docker backend (operator-managed):"
}

test_portable_docs_call_out_headful_xvfb_not_headless() {
  assert_file_contains "${README_FILE}" "portable Xvfb runtime"
  assert_file_contains "${INSTALL_FILE}" "bundled Xvfb"
  assert_file_contains "${INSTALL_FILE}" 'Do not install from `npx` or a raw source'
}

test_archive_names_are_explicit() {
  assert_file_contains "${INSTALL_FILE}" "clawbrowser-linux-arm64.tar.gz"
  assert_file_contains "${INSTALL_FILE}" "clawbrowser-portable-linux-arm64-glibc.tar.gz"
  assert_file_contains "${INSTALL_FILE}" "The normal Linux release archive is not the portable runtime payload."
}

test_cdp_attach_flow_is_documented() {
  assert_file_contains "${INSTALL_FILE}" "clawctl --cdp http://127.0.0.1:9222 tabs list --json"
  assert_file_contains "${README_FILE}" "clawctl --cdp http://127.0.0.1:9222 tabs list --json"
}

run_test "portable is default Linux path" test_portable_is_default_linux_path
run_test "restricted container messaging points to portable first" test_restricted_container_message_points_to_portable_first
run_test "docker is optional operator-managed" test_docker_is_optional_operator_managed
run_test "portable docs call out headful Xvfb and not headless" test_portable_docs_call_out_headful_xvfb_not_headless
run_test "archive names are explicit" test_archive_names_are_explicit
run_test "CDP attach flow is documented" test_cdp_attach_flow_is_documented
