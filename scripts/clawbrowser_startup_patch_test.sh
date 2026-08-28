#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
STARTUP_PATCH="${REPO_ROOT}/clawbrowser/patches/001-browser-main-init.patch"
BUILD_DEP_PATCH="${REPO_ROOT}/clawbrowser/patches/020-build-dep.patch"

fail() {
  printf 'FAIL: %s\n' "$*" >&2
  exit 1
}

assert_contains() {
  local file="$1"
  local needle="$2"
  grep -Fq -- "${needle}" "${file}" || \
    fail "expected ${file} to contain: ${needle}"
}

assert_order() {
  local file="$1"
  local first="$2"
  local second="$3"
  local first_line
  local second_line

  first_line="$(grep -Fn -- "${first}" "${file}" | head -n1 | cut -d: -f1 || true)"
  second_line="$(grep -Fn -- "${second}" "${file}" | head -n1 | cut -d: -f1 || true)"
  [[ -n "${first_line}" ]] || fail "missing ordered marker: ${first}"
  [[ -n "${second_line}" ]] || fail "missing ordered marker: ${second}"
  (( first_line < second_line )) || \
    fail "expected '${first}' before '${second}' in ${file}"
}

assert_contains "${STARTUP_PATCH}" \
  "diff --git a/chrome/app/chrome_main_delegate.cc b/chrome/app/chrome_main_delegate.cc"
assert_contains "${STARTUP_PATCH}" \
  '#include "clawbrowser/startup.h"'
assert_contains "${STARTUP_PATCH}" \
  "clawbrowser::ConfigureCommandLineBeforeUserDataDir("
assert_contains "${STARTUP_PATCH}" \
  "crash_reporter::InitializeCrashKeys();"
assert_order "${STARTUP_PATCH}" \
  "clawbrowser::ConfigureCommandLineBeforeUserDataDir(" \
  "crash_reporter::InitializeCrashKeys();"
assert_contains "${STARTUP_PATCH}" \
  "auto basic_startup_result = clawbrowser::HandleBasicStartupComplete("
assert_contains "${STARTUP_PATCH}" \
  "SetupTracing();"
assert_order "${STARTUP_PATCH}" \
  "auto basic_startup_result = clawbrowser::HandleBasicStartupComplete(" \
  "SetupTracing();"
assert_contains "${STARTUP_PATCH}" \
  "auto early_startup_result = clawbrowser::ConfigureEarlyStartup("
assert_contains "${STARTUP_PATCH}" \
  "browser_process_ = std::make_unique<BrowserProcessImpl>(startup_data_);"
assert_order "${STARTUP_PATCH}" \
  "auto early_startup_result = clawbrowser::ConfigureEarlyStartup(" \
  "browser_process_ = std::make_unique<BrowserProcessImpl>(startup_data_);"
assert_contains "${BUILD_DEP_PATCH}" \
  "diff --git a/chrome/BUILD.gn b/chrome/BUILD.gn"
assert_contains "${BUILD_DEP_PATCH}" \
  '"//clawbrowser:clawbrowser_core",'

printf 'PASS\n'
