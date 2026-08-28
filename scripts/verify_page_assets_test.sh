#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
VERIFY_HTML="${REPO_ROOT}/clawbrowser/verify/resources/verify.html"
VERIFY_CSS="${REPO_ROOT}/clawbrowser/verify/resources/verify.css"

fail() {
  printf 'FAIL: %s\n' "$*" >&2
  exit 1
}

assert_contains() {
  local haystack="$1"
  local needle="$2"
  if [[ "${haystack}" != *"${needle}"* ]]; then
    fail "expected to find '${needle}'"
  fi
}

verify_html="$(<"${VERIFY_HTML}")"
verify_css="$(<"${VERIFY_CSS}")"

assert_contains "${verify_html}" '<colgroup>'
assert_contains "${verify_html}" '<col class="surface-column">'
assert_contains "${verify_html}" '<col class="status-column">'
assert_contains "${verify_html}" '<col class="expected-column">'
assert_contains "${verify_html}" '<col class="actual-column">'
assert_contains "${verify_html}" '<script src="verify_timezones.js"></script>'
assert_contains "${verify_html}" '<script src="verify.js"></script>'

assert_contains "${verify_css}" '#results th:nth-child(1),'
assert_contains "${verify_css}" '#results td:nth-child(1) {'
assert_contains "${verify_css}" 'white-space: nowrap;'
assert_contains "${verify_css}" '#results th:nth-child(2),'
assert_contains "${verify_css}" '#results td:nth-child(2) {'
assert_contains "${verify_css}" '#results th:nth-child(n + 3),'
assert_contains "${verify_css}" '#results td:nth-child(n + 3) {'
assert_contains "${verify_css}" 'overflow-wrap: anywhere;'

printf 'PASS\n'
