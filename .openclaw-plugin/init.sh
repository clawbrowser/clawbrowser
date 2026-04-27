#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_ROOT="${OPENCLAW_STATE_DIR:-${HOME}/.openclaw}"
PLUGIN_STATE_DIR="${STATE_ROOT}/plugins/openclaw-plugin"
CONFIG_FILE="${OPENCLAW_PLUGIN_CONFIG_FILE:-${PLUGIN_STATE_DIR}/config.json}"
MODE="${OPENCLAW_PLUGIN_MODE:-runtime}"

mkdir -p "${PLUGIN_STATE_DIR}"

if [[ ! -f "${CONFIG_FILE}" ]]; then
  cat > "${CONFIG_FILE}" <<'JSON'
{
  "gatewayUrl": "ws://127.0.0.1:18789",
  "sessionName": "main",
  "clawbrowserSession": "openclaw-main",
  "startCommand": "clawbrowser start --session openclaw-main --",
  "startUrlPlaceholder": "${url}",
  "endpointCommand": "clawbrowser endpoint --session openclaw-main",
  "rotateCommand": "clawbrowser rotate --session openclaw-main",
  "verifyCommand": "clawbrowser start --session openclaw-main -- clawbrowser://verify/",
  "closeBlankTabs": true,
  "notes": [
    "No secrets in this file.",
    "Browser API keys must stay in browser-managed config.json.",
    "Resolve config paths before writing; do not pass unresolved shell-expression paths to file-write tools. They may create literal workspace paths instead of the real config file."
  ]
}
JSON
  chmod 600 "${CONFIG_FILE}" 2>/dev/null || true
  echo "[openclaw-plugin] created ${CONFIG_FILE}"
else
  echo "[openclaw-plugin] using existing ${CONFIG_FILE}"
fi

if [[ "${MODE}" == "install" ]]; then
  echo "[openclaw-plugin] install bootstrap complete"
fi
