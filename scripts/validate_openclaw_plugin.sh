#!/usr/bin/env bash
set -euo pipefail

INSTALL_ROOT="${CLAWBROWSER_INSTALL_ROOT:-${HOME}/.clawbrowser}"
INSTALL_BIN="${CLAWBROWSER_INSTALL_BIN:-${HOME}/.local/bin}"
RUN_CDP=0
SESSION_NAME="openclaw-plugin-validation"

usage() {
  cat <<'EOF'
Usage:
  bash scripts/validate_openclaw_plugin.sh [--mock-cdp] [--run-cdp] [--session <name>]

Checks:
  - plugin directory and manifest files exist
  - launcher symlinks exist
  - CDP endpoint check (mocked by default; live check with --run-cdp)
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --run-cdp)
      RUN_CDP=1
      shift
      ;;
    --mock-cdp)
      RUN_CDP=0
      shift
      ;;
    --session)
      [[ $# -ge 2 ]] || { echo "--session requires a value" >&2; exit 1; }
      SESSION_NAME="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage
      exit 1
      ;;
  esac
done

failures=0

check_exists() {
  local path="$1"
  if [[ -e "${path}" ]]; then
    echo "OK   exists: ${path}"
  else
    echo "FAIL missing: ${path}" >&2
    failures=$((failures + 1))
  fi
}

check_symlink() {
  local path="$1"
  if [[ -L "${path}" ]]; then
    echo "OK   symlink: ${path} -> $(readlink "${path}")"
  else
    echo "FAIL missing symlink: ${path}" >&2
    failures=$((failures + 1))
  fi
}

PLUGIN_DIR="${INSTALL_ROOT}/.openclaw-plugin"

check_exists "${PLUGIN_DIR}"
check_exists "${PLUGIN_DIR}/plugin.json"
check_exists "${PLUGIN_DIR}/init.sh"
check_exists "${PLUGIN_DIR}/SKILL.md"

check_symlink "${INSTALL_BIN}/clawbrowser"
check_symlink "${INSTALL_BIN}/clawbrowser-mcp"
check_symlink "${INSTALL_BIN}/openclaw-plugin-init"

if [[ "${RUN_CDP}" -eq 1 ]]; then
  if [[ ! -x "${INSTALL_BIN}/clawbrowser" ]]; then
    echo "FAIL unable to run CDP check; launcher missing: ${INSTALL_BIN}/clawbrowser" >&2
    failures=$((failures + 1))
  else
    endpoint="$("${INSTALL_BIN}/clawbrowser" endpoint --session "${SESSION_NAME}" 2>/dev/null || true)"
    if [[ "${endpoint}" == ws://* || "${endpoint}" == wss://* || "${endpoint}" == http://* || "${endpoint}" == https://* ]]; then
      echo "OK   live CDP endpoint: ${endpoint}"
    else
      echo "FAIL live CDP endpoint not available for session '${SESSION_NAME}'" >&2
      failures=$((failures + 1))
    fi
  fi
else
  if [[ -x "${INSTALL_BIN}/clawbrowser" ]]; then
    echo "OK   mock CDP check: clawbrowser launcher present"
  else
    echo "FAIL mock CDP check: clawbrowser launcher missing" >&2
    failures=$((failures + 1))
  fi
fi

if [[ "${failures}" -gt 0 ]]; then
  echo "Validation failed with ${failures} issue(s)." >&2
  exit 1
fi

echo "Validation passed."
