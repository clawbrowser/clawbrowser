#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOCKERFILE="${REPO_ROOT}/docker/clawbrowser-runtime.Dockerfile"
ENTRYPOINT="${REPO_ROOT}/docker/docker-entrypoint.sh"

assert_contains() {
  local haystack="$1"
  local needle="$2"
  if [[ "${haystack}" != *"${needle}"* ]]; then
    printf 'expected to find %q\n' "${needle}" >&2
    exit 1
  fi
}

assert_not_contains() {
  local haystack="$1"
  local needle="$2"
  if [[ "${haystack}" == *"${needle}"* ]]; then
    printf 'did not expect to find %q\n' "${needle}" >&2
    exit 1
  fi
}

dockerfile_contents="$(<"${DOCKERFILE}")"
entrypoint_contents="$(<"${ENTRYPOINT}")"

assert_contains "${entrypoint_contents}" 'Xvfb'
assert_contains "${entrypoint_contents}" 'dbus-run-session'

assert_not_contains "${dockerfile_contents}" 'chromium-sandbox'
assert_not_contains "${dockerfile_contents}" '/usr/lib/chromium/chrome-sandbox'
assert_contains "${dockerfile_contents}" 'CLAWBROWSER_NO_SANDBOX=1'
assert_not_contains "${entrypoint_contents}" 'CHROME_DEVEL_SANDBOX'
assert_not_contains "${entrypoint_contents}" '/usr/lib/chromium/chrome-sandbox'
assert_not_contains "${entrypoint_contents}" '/opt/clawbrowser/chrome_sandbox'
assert_not_contains "${entrypoint_contents}" '/opt/clawbrowser/chrome-sandbox'

printf 'PASS\n'
