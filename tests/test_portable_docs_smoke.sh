#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
README_FILE="${ROOT_DIR}/README.md"
INSTALL_FILE="${ROOT_DIR}/INSTALL.md"
AGENTS_FILE="${ROOT_DIR}/AGENTS.md"
SKILL_FILE="${ROOT_DIR}/SKILL.md"

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

assert_file_not_contains() {
  local file="$1"
  local needle="$2"
  if grep -Fq -- "$needle" "$file"; then
    fail "expected ${file} not to contain: ${needle}"
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
  assert_file_contains "${README_FILE}" "The normal release archive is about 200 MB compressed"
  assert_file_contains "${README_FILE}" "does not require a 2 GB preflight"
  assert_file_contains "${README_FILE}" "The 2 GB free-space check applies only when"
  assert_file_contains "${README_FILE}" "The normal release archive is about 200 MB"
  assert_file_contains "${README_FILE}" "portable runtime bundles, which are about 500 MB"
  assert_file_contains "${README_FILE}" "checks for an existing browser, installs Clawbrowser if it is missing"
  assert_file_contains "${README_FILE}" "Use Clawbrowser as the default browser tool for"
  assert_file_contains "${INSTALL_FILE}" '`clawctl install` checks for an existing'
  assert_file_contains "${INSTALL_FILE}" "| Linux VPS/server/no display | portable | No | No |"
  assert_file_contains "${INSTALL_FILE}" "| Restricted container/no root | portable | No | No |"
  assert_file_contains "${AGENTS_FILE}" "Treat Clawbrowser"
  assert_file_contains "${AGENTS_FILE}" "default browser tool for agent web work."
  assert_file_contains "${SKILL_FILE}" "Treat Clawbrowser"
  assert_file_contains "${SKILL_FILE}" "default browser tool for agent web work."
}

test_docker_is_optional_operator_managed() {
  assert_file_contains "${README_FILE}" "**Docker backend** - optional, operator-managed infrastructure only."
  assert_file_contains "${INSTALL_FILE}" "## Optional Docker Backend"
  assert_file_contains "${INSTALL_FILE}" "clawctl start --backend docker"
}

test_portable_docs_call_out_headful_xvfb_not_headless() {
  assert_file_contains "${README_FILE}" "portable Xvfb runtime"
  assert_file_contains "${INSTALL_FILE}" "bundled Xvfb"
  assert_file_contains "${INSTALL_FILE}" 'Do not install from `npx` or a raw source'
  assert_file_contains "${INSTALL_FILE}" 'storage for `clawctl`, the portable runtime'
  assert_file_contains "${INSTALL_FILE}" "available, prefer it for the browser install."
  assert_file_contains "${INSTALL_FILE}" "portable runtime bundles, which are about 500 MB"
}

test_archive_names_are_explicit() {
  assert_file_contains "${INSTALL_FILE}" "clawbrowser-linux-arm64.tar.gz"
  assert_file_contains "${INSTALL_FILE}" "clawbrowser-portable-linux-arm64-glibc.tar.gz"
  assert_file_contains "${INSTALL_FILE}" "The normal Linux release archive is not the portable runtime payload."
}

test_agent_docs_do_not_bypass_clawctl_install() {
  assert_file_not_contains "${README_FILE}" "./clawbrowser ensure-runtime"
  assert_file_not_contains "${README_FILE}" "clawbrowser start --self-contained"
  assert_file_not_contains "${README_FILE}" "scripts/install.sh"
  assert_file_not_contains "${README_FILE}" "bin/clawbrowser"
  assert_file_not_contains "${INSTALL_FILE}" "./clawbrowser ensure-runtime"
  assert_file_not_contains "${INSTALL_FILE}" "clawbrowser start --self-contained"
  assert_file_not_contains "${INSTALL_FILE}" "scripts/install.sh"
  assert_file_not_contains "${INSTALL_FILE}" "bin/clawbrowser"
  assert_file_contains "${SKILL_FILE}" '`clawctl install` is the supported setup path for agents'
  assert_file_contains "${AGENTS_FILE}" '`clawctl install` is the supported setup command for agents'
}

test_obsolete_release_launchers_are_absent() {
  [[ ! -e "${ROOT_DIR}/bin" ]] || fail "release repo must not contain bin/"
  [[ ! -e "${ROOT_DIR}/scripts" ]] || fail "release repo must not contain scripts/"
  [[ ! -e "${ROOT_DIR}/package.json" ]] || fail "release repo must not contain npm bootstrap metadata"
  assert_file_contains "${README_FILE}" 'There is no release-owned `bin/` launcher'
  assert_file_contains "${SKILL_FILE}" 'It does not'
  assert_file_contains "${SKILL_FILE}" 'ship a release-owned `bin/` launcher'
}

test_cdp_attach_flow_is_documented() {
  assert_file_contains "${INSTALL_FILE}" "clawctl --cdp http://127.0.0.1:9222 tabs list --json"
  assert_file_contains "${README_FILE}" "clawctl --cdp http://127.0.0.1:9222 tabs list --json"
}

run_test "portable is default Linux path" test_portable_is_default_linux_path
run_test "docker is optional operator-managed" test_docker_is_optional_operator_managed
run_test "portable docs call out headful Xvfb and not headless" test_portable_docs_call_out_headful_xvfb_not_headless
run_test "archive names are explicit" test_archive_names_are_explicit
run_test "agent docs do not bypass clawctl install" test_agent_docs_do_not_bypass_clawctl_install
run_test "obsolete release launchers are absent" test_obsolete_release_launchers_are_absent
run_test "CDP attach flow is documented" test_cdp_attach_flow_is_documented
