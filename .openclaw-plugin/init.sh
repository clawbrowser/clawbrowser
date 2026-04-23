#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_ROOT="${OPENCLAW_STATE_DIR:-${HOME}/.openclaw}"
PLUGIN_STATE_DIR="${STATE_ROOT}/plugins/openclaw-plugin"
CONFIG_FILE="${OPENCLAW_PLUGIN_CONFIG_FILE:-${PLUGIN_STATE_DIR}/config.json}"
WORKSPACE_DIR="${OPENCLAW_WORKSPACE_DIR:-${STATE_ROOT}/workspace}"
MODE="${OPENCLAW_PLUGIN_MODE:-runtime}"

mkdir -p "${PLUGIN_STATE_DIR}"

if [[ ! -f "${CONFIG_FILE}" ]]; then
  cat > "${CONFIG_FILE}" <<'JSON'
{
  "gatewayUrl": "ws://127.0.0.1:18789",
  "sessionName": "main",
  "clawbrowser": {
    "session": "openclaw-main",
    "fingerprint": true,
    "verifyOnStart": false
  },
  "notes": [
    "No secrets in this file.",
    "Browser API keys must stay in browser-managed config.json."
  ]
}
JSON
  chmod 600 "${CONFIG_FILE}" 2>/dev/null || true
  echo "[openclaw-plugin] created ${CONFIG_FILE}"
else
  echo "[openclaw-plugin] using existing ${CONFIG_FILE}"
fi

if [[ -f "${WORKSPACE_DIR}/USER.md" ]]; then
  if grep -q "<required:" "${WORKSPACE_DIR}/USER.md"; then
    cat <<'EOF'
[openclaw-plugin] USER.md still has placeholders.
Please provide:
  1) name
  2) pronouns
  3) timezone (IANA format, e.g. Europe/Prague)
EOF
  fi
fi

if [[ "${MODE}" == "install" ]]; then
  echo "[openclaw-plugin] install bootstrap complete"
fi
