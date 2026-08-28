#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
AUTH_HTML="${REPO_ROOT}/clawbrowser/auth/resources/auth.html"
AUTH_CSS="${REPO_ROOT}/clawbrowser/auth/resources/auth.css"
AUTH_JS="${REPO_ROOT}/clawbrowser/auth/resources/auth.js"

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

assert_not_contains() {
  local haystack="$1"
  local needle="$2"
  if [[ "${haystack}" == *"${needle}"* ]]; then
    fail "expected not to find '${needle}'"
  fi
}

auth_html="$(<"${AUTH_HTML}")"
auth_css="$(<"${AUTH_CSS}")"
auth_js="$(<"${AUTH_JS}")"

assert_contains "${auth_html}" '<img class="brand-icon" src="side-bite.svg" alt="">'
assert_contains "${auth_html}" '<button id="get-api-key"'
assert_contains "${auth_html}" 'class="secondary get-key"'
assert_not_contains "${auth_html}" '<a id="get-api-key"'
assert_contains "${auth_html}" '<span>Paste API key</span>'
assert_not_contains "${auth_html}" 'Open dashboard'
assert_not_contains "${auth_html}" 'Paste your Clawbrowser dashboard key to continue.'

python3 - "${AUTH_HTML}" <<'PY'
import pathlib
import sys

html = pathlib.Path(sys.argv[1]).read_text()
get_key = html.index('id="get-api-key"')
field = html.index('for="api-key"')
if get_key > field:
    raise SystemExit("Get API key must be shown before the API-key input")
PY

assert_contains "${auth_css}" '--secondary-border: rgba(255, 255, 255, 0.22);'
assert_contains "${auth_css}" '--secondary-border: rgba(17, 17, 17, 0.22);'
assert_contains "${auth_css}" 'border: 1px solid var(--secondary-border);'
assert_contains "${auth_css}" 'grid-template-columns: 1fr;'
assert_contains "${auth_css}" 'margin: 0 0 14px;'
assert_contains "${auth_css}" 'margin: 0 0 28px;'
assert_contains "${auth_css}" 'margin: 0 0 34px;'

node - "${AUTH_JS}" <<'JS'
const fs = require('fs');
const vm = require('vm');

const source = fs.readFileSync(process.argv[2], 'utf8');

function makeElement() {
  return {
    disabled: false,
    value: '',
    className: '',
    textContent: '',
    handlers: new Map(),
    addEventListener(type, handler) {
      this.handlers.set(type, handler);
    },
    focus() {},
  };
}

const elements = new Map([
  ['api-key', makeElement()],
  ['status', makeElement()],
  ['get-api-key', makeElement()],
  ['save-api-key', makeElement()],
]);

const sendCalls = [];
const context = vm.createContext({
  chrome: {
    send(name, args) {
      sendCalls.push({name, args});
    },
  },
  document: {
    getElementById(id) {
      return elements.get(id) || null;
    },
  },
  window: {
    setTimeout() {},
  },
});

vm.runInContext(source, context, {filename: process.argv[2]});
const clickHandler = elements.get('get-api-key').handlers.get('click');
if (typeof clickHandler !== 'function') {
  throw new Error('missing Get API key click handler');
}
clickHandler();
if (sendCalls.length !== 1 || sendCalls[0].name !== 'openDashboard') {
  throw new Error('Get API key should send openDashboard exactly once');
}
JS

assert_not_contains "${auth_js}" 'window.open('
assert_not_contains "${auth_js}" 'location.assign('
assert_contains "${auth_js}" "chrome.send('openDashboard')"

printf 'PASS\n'
