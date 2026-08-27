#!/usr/bin/env bash
# Guards the WebUI security boundary in patch 019.
#
# ShouldIncludeDataSource() decides which WebUI data sources a page may load
# in process. Clawbrowser needs clawbrowser:// pages to load their own data
# source, but that relaxation must stay scoped to the clawbrowser scheme. If
# the same-origin check is hoisted above the kChromeUIScheme gate it applies to
# every scheme, and chrome-untrusted:// data sources start passing where
# upstream Chromium rejects them.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
VERIFY_PAGE_PATCH="${REPO_ROOT}/clawbrowser/patches/019-verify-page-registration.patch"

fail() {
  printf 'FAIL: %s\n' "$*" >&2
  exit 1
}

assert_line() {
  local file="$1"
  local line="$2"
  grep -Fxq -- "${line}" "${file}" || \
    fail "expected $(basename "${file}") to contain the line: ${line}"
}

assert_no_line() {
  local file="$1"
  local line="$2"
  if grep -Fxq -- "${line}" "${file}"; then
    fail "$(basename "${file}") must not contain the line: ${line}"
  fi
}

assert_order() {
  local file="$1"
  local first="$2"
  local second="$3"
  local first_line
  local second_line

  first_line="$(grep -Fxn -- "${first}" "${file}" | head -n1 | cut -d: -f1 || true)"
  second_line="$(grep -Fxn -- "${second}" "${file}" | head -n1 | cut -d: -f1 || true)"
  [[ -n "${first_line}" ]] || fail "missing ordered marker: ${first}"
  [[ -n "${second_line}" ]] || fail "missing ordered marker: ${second}"
  (( first_line < second_line )) || \
    fail "expected '${first}' before '${second}' in $(basename "${file}")"
}

[[ -f "${VERIFY_PAGE_PATCH}" ]] || fail "missing patch: ${VERIFY_PAGE_PATCH}"

assert_line "${VERIFY_PAGE_PATCH}" \
  "--- a/content/browser/webui/web_ui_impl.cc"

# The relaxed path must be reachable only when the clawbrowser scheme is on one
# side of the comparison, and must still require an exact origin match.
assert_line "${VERIFY_PAGE_PATCH}" \
  '+  if (origin.scheme() == "clawbrowser" ||'
assert_line "${VERIFY_PAGE_PATCH}" \
  '+      current_origin.scheme() == "clawbrowser") {'
assert_line "${VERIFY_PAGE_PATCH}" \
  "+    return origin == current_origin;"
assert_order "${VERIFY_PAGE_PATCH}" \
  '+  if (origin.scheme() == "clawbrowser" ||' \
  "   if (origin.scheme() != kChromeUIScheme) {"

# The upstream rule must survive untouched as context: the kChromeUIScheme gate
# still rejects every other scheme. The shared-resource return sits outside the
# hunk entirely, which is itself the proof it is unmodified -- the assert_no_line
# checks below cover the case where a future edit pulls it in as a change.
assert_line "${VERIFY_PAGE_PATCH}" \
  "   // We only support data sources that serve URLs of the form: chrome://*"
assert_line "${VERIFY_PAGE_PATCH}" \
  "   if (origin.scheme() != kChromeUIScheme) {"

# Regression: a scheme-agnostic same-origin shortcut above the gate would let
# chrome-untrusted:// data sources through.
assert_no_line "${VERIFY_PAGE_PATCH}" \
  "+  if (origin == current_origin) {"
assert_no_line "${VERIFY_PAGE_PATCH}" \
  "-  // We only support data sources that serve URLs of the form: chrome://*"
assert_no_line "${VERIFY_PAGE_PATCH}" \
  "-  if (origin.scheme() != kChromeUIScheme) {"
assert_no_line "${VERIFY_PAGE_PATCH}" \
  "-  return origin == current_origin || origin.host() == kChromeUIResourcesHost ||"

printf 'PASS\n'
