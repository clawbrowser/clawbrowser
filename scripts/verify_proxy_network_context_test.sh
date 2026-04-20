#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
VERIFY_PAGE_CC="${REPO_ROOT}/clawbrowser/verify/verify_page.cc"

assert_contains() {
  local haystack="$1"
  local needle="$2"
  if [[ "${haystack}" != *"${needle}"* ]]; then
    printf 'Expected to find: %s\n' "${needle}" >&2
    exit 1
  fi
}

verify_page_cc="$(<"${VERIFY_PAGE_CC}")"

assert_contains "${verify_page_cc}" 'system_network_context_manager()'
assert_contains "${verify_page_cc}" 'GetSharedURLLoaderFactory()'

printf 'verify proxy network context test passed\n'
