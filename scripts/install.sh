#!/usr/bin/env bash
set -euo pipefail

SOURCE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPOSITORY="clawbrowser/clawbrowser"
INSTALL_ROOT="${CLAWBROWSER_INSTALL_ROOT:-${HOME}/.clawbrowser}"
INSTALL_BIN="${CLAWBROWSER_INSTALL_BIN:-${HOME}/.local/bin}"
CODEX_PLUGINS_ROOT="${CLAWBROWSER_CODEX_PLUGINS_ROOT:-${HOME}/.codex/plugins}"
CLAUDE_PLUGINS_ROOT="${CLAWBROWSER_CLAUDE_PLUGINS_ROOT:-${HOME}/.claude/plugins}"
AGENTS_PLUGINS_ROOT="${CLAWBROWSER_AGENTS_PLUGINS_ROOT:-${HOME}/.agents/plugins}"
HERMES_PLUGINS_ROOT="${CLAWBROWSER_HERMES_PLUGINS_ROOT:-${HOME}/.hermes/plugins}"
GEMINI_EXTENSIONS_ROOT="${CLAWBROWSER_GEMINI_EXTENSIONS_ROOT:-${HOME}/.gemini/extensions}"
RUNTIME_IMAGE="${CLAWBROWSER_RUNTIME_IMAGE:-docker.io/clawbrowser/clawbrowser:latest}"
RELEASE_REF="${CLAWBROWSER_RELEASE_REF:-latest}"
TARGET="${CLAWBROWSER_TARGET:-auto}"

log() {
  printf '[clawbrowser-install] %s\n' "$*"
}

die() {
  printf '[clawbrowser-install] ERROR: %s\n' "$*" >&2
  exit 1
}

usage() {
  cat <<'EOF'
Usage:
  bash scripts/install.sh [auto|codex|claude|gemini|hermes|openclaw]
  bash scripts/install.sh --target <auto|codex|claude|gemini|hermes|openclaw>

Environment overrides:
  CLAWBROWSER_TARGET         Install target, default: auto
  CLAWBROWSER_INSTALL_ROOT   Bundle install root, default: ~/.clawbrowser
  CLAWBROWSER_INSTALL_BIN    Command install directory, default: ~/.local/bin
  CLAWBROWSER_CLAUDE_PLUGINS_ROOT  Claude plugin directory, default: ~/.claude/plugins
  CLAWBROWSER_GEMINI_EXTENSIONS_ROOT  Gemini extension directory, default: ~/.gemini/extensions
  CLAWBROWSER_HERMES_PLUGINS_ROOT  Hermes plugin directory, default: ~/.hermes/plugins
  CLAWBROWSER_RUNTIME_IMAGE  Docker image used at runtime when no native app bundle exists, default: docker.io/clawbrowser/clawbrowser:latest
  CLAWBROWSER_RELEASE_REF    Release ref or tag, default: latest
  CLAWBROWSER_APP_PATH       Optional macOS Clawbrowser.app path or executable

This script expects an assembled release bundle that already contains
bin/clawctl, bin/clawbrowser, and bin/clawbrowser-mcp. In normal installs,
clawctl invokes this script after the release bundle is unpacked. Do not run
this script from a raw source checkout.

The browser-managed config.json is reused automatically once saved. If the
saved config is missing, the launcher prompts once and writes the key into
the browser-managed config. Resolve config paths before writing; do not pass
`${XDG_CONFIG_HOME:-$HOME/.config}/...` directly to file/write tools; they
may create literal workspace paths instead of the real config file. Use
clawbrowser://auth only for manual browser setup or reauthentication. Do not
put the API key in MCP config, agent-side config files, shell startup scripts,
or logs; let the browser manage its own config storage.
EOF
}

is_source_root_ready() {
  [[ -x "${SOURCE_ROOT}/bin/clawctl" ]] &&
    [[ -x "${SOURCE_ROOT}/bin/clawbrowser" ]] &&
    [[ -x "${SOURCE_ROOT}/bin/clawbrowser-mcp" ]] &&
    [[ -f "${SOURCE_ROOT}/plugins/.claude-plugin/plugin.json" ]] &&
    [[ -f "${SOURCE_ROOT}/plugins/.codex-plugin/plugin.json" ]] &&
    [[ -f "${SOURCE_ROOT}/plugins/.openclaw-plugin/plugin.json" ]] &&
    [[ -f "${SOURCE_ROOT}/plugins/.hermes-plugin/plugin.yaml" ]] &&
    [[ -f "${SOURCE_ROOT}/gemini-extension.json" ]] &&
    [[ -f "${SOURCE_ROOT}/GEMINI.md" ]] &&
    [[ -f "${SOURCE_ROOT}/AGENTS.md" ]] &&
    [[ -f "${SOURCE_ROOT}/SKILL.md" ]] &&
    [[ -f "${SOURCE_ROOT}/scripts/install.sh" ]]
}

ensure_source_root() {
  if ! is_source_root_ready; then
    die "This installer must run from an assembled release bundle containing bin/clawctl, bin/clawbrowser, bin/clawbrowser-mcp, plugin manifests, AGENTS.md, GEMINI.md, SKILL.md, and scripts/install.sh. Raw source checkouts and npx installs do not include the browser runtime; download a release archive and run bundled ./clawctl install instead. Install guide: https://github.com/clawbrowser/clawbrowser/blob/main/INSTALL.md"
  fi
}

normalize_target() {
  case "${1}" in
    auto|codex|claude|gemini|hermes|openclaw) printf '%s\n' "${1}" ;;
    *)
      die "Unknown target: ${1}"
      ;;
  esac
}

ancestor_process_matches() {
  local pattern="$1"
  local pid="${PPID:-}"
  local command parent

  while [[ -n "${pid}" && "${pid}" != "0" ]]; do
    command="$(ps -p "${pid}" -o comm= 2>/dev/null || true)"
    if [[ "${command}" == *"${pattern}"* ]]; then
      return 0
    fi
    parent="$(ps -p "${pid}" -o ppid= 2>/dev/null | tr -d ' ' || true)"
    if [[ -z "${parent}" || "${parent}" == "${pid}" ]]; then
      break
    fi
    pid="${parent}"
  done

  return 1
}

detect_target() {
  if [[ -n "${HERMES_SESSION_ID:-}" || -n "${HERMES_HOME:-}" ]] || ancestor_process_matches "hermes"; then
    printf '%s\n' hermes
    return 0
  fi

  if [[ -n "${CODEX_SANDBOX:-}" || -n "${CODEX_HOME:-}" ]] || ancestor_process_matches "codex"; then
    printf '%s\n' codex
    return 0
  fi

  if [[ -n "${CLAUDECODE:-}" || -n "${CLAUDE_CODE_ENTRYPOINT:-}" ]] || ancestor_process_matches "claude"; then
    printf '%s\n' claude
    return 0
  fi

  if [[ -n "${GEMINI_CLI:-}" ]] || ancestor_process_matches "gemini"; then
    printf '%s\n' gemini
    return 0
  fi

  if command -v hermes >/dev/null 2>&1; then
    printf '%s\n' hermes
    return 0
  fi

  if command -v codex >/dev/null 2>&1; then
    printf '%s\n' codex
    return 0
  fi

  if command -v claude >/dev/null 2>&1; then
    printf '%s\n' claude
    return 0
  fi

  if command -v gemini >/dev/null 2>&1; then
    printf '%s\n' gemini
    return 0
  fi

  printf '%s\n' hermes
}

resolve_target() {
  if [[ "${TARGET}" == "auto" ]]; then
    TARGET="$(detect_target)"
    log "Auto-detected install target: ${TARGET}"
  fi
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "${1}" in
      -h|--help)
        usage
        exit 0
        ;;
      --target|-t)
        [[ $# -ge 2 ]] || die "${1} requires a target name"
        TARGET="$(normalize_target "${2}")"
        shift 2
        ;;
      install)
        shift
        ;;
      auto|codex|claude|gemini|hermes|openclaw)
        TARGET="$(normalize_target "${1}")"
        shift
        ;;
      --)
        shift
        break
        ;;
      *)
        die "Unknown argument: ${1}"
        ;;
    esac
  done

  if [[ $# -gt 0 ]]; then
    die "Unexpected extra arguments: $*"
  fi
}

require_command() {
  local name="$1"
  if ! command -v "${name}" >/dev/null 2>&1; then
    die "Required command not found: ${name}"
  fi
}

absolute_path() {
  python3 - "$1" <<'PY'
import os
import pathlib
import sys

print(pathlib.Path(os.path.expanduser(sys.argv[1])).resolve())
PY
}

host_arch() {
  case "$(uname -m)" in
    arm64|aarch64) printf '%s\n' arm64 ;;
    x86_64|amd64) printf '%s\n' x64 ;;
    *)
      die "Unsupported architecture: $(uname -m)"
      ;;
  esac
}

release_asset_base() {
  if [[ "${RELEASE_REF}" == "latest" ]]; then
    printf 'https://github.com/clawbrowser/clawbrowser/releases/latest/download'
  else
    printf 'https://github.com/clawbrowser/clawbrowser/releases/download/%s' "${RELEASE_REF}"
  fi
}

is_macos() {
  [[ "$(uname -s)" == "Darwin" ]]
}

resolve_app_binary() {
  local candidate bundle

  if [[ -n "${CLAWBROWSER_APP_PATH:-}" ]]; then
    if [[ "${CLAWBROWSER_APP_PATH}" == *.app ]]; then
      candidate="${CLAWBROWSER_APP_PATH}/Contents/MacOS/Clawbrowser"
    else
      candidate="${CLAWBROWSER_APP_PATH}"
    fi
    if [[ -x "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  fi

  if ! is_macos; then
    return 1
  fi

  for bundle in \
    "${INSTALL_ROOT}/Clawbrowser.app" \
    "${HOME}/.clawbrowser/Clawbrowser.app" \
    "${HOME}/Desktop/Clawbrowser.app" \
    "${HOME}/Downloads/Clawbrowser.app" \
    "${HOME}/Applications/Clawbrowser.app" \
    "/Applications/Clawbrowser.app"
  do
    candidate="${bundle}/Contents/MacOS/Clawbrowser"
    if [[ -x "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  return 1
}

resolve_app_bundle() {
  local candidate
  if [[ -n "${CLAWBROWSER_APP_PATH:-}" ]]; then
    if [[ "${CLAWBROWSER_APP_PATH}" == *.app ]]; then
      candidate="${CLAWBROWSER_APP_PATH}"
    else
      candidate="$(dirname "${CLAWBROWSER_APP_PATH}")"
    fi
    if [[ -d "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  fi

  if ! is_macos; then
    return 1
  fi

  for candidate in \
    "${INSTALL_ROOT}/Clawbrowser.app" \
    "${HOME}/Desktop/Clawbrowser.app" \
    "${HOME}/Downloads/Clawbrowser.app" \
    "${HOME}/Applications/Clawbrowser.app" \
    "/Applications/Clawbrowser.app"
  do
    if [[ -d "${candidate}" ]]; then
      python3 - "$candidate" <<'PY'
import os
import sys

print(os.path.realpath(sys.argv[1]))
PY
      return 0
    fi
  done

  return 1
}

native_app_asset_name() {
  if ! is_macos; then
    return 1
  fi

  case "$(host_arch)" in
    arm64) printf '%s\n' clawbrowser-macos-arm64.tar.gz ;;
    *) return 1 ;;
  esac
}

download_release_asset() {
  local asset_name="$1"
  local destination="$2"

  require_command curl
  curl -fsSL \
    "$(release_asset_base)/${asset_name}" \
    -o "${destination}"
}

download_native_app_bundle() {
  local asset_name tmp_dir archive_path bundle_path

  asset_name="$(native_app_asset_name)" || return 1
  require_command tar
  require_command mktemp

  tmp_dir="$(mktemp -d)"
  archive_path="${tmp_dir}/${asset_name}"

  log "Downloading native app bundle ${asset_name}"
  if ! download_release_asset "${asset_name}" "${archive_path}"; then
    rm -rf "${tmp_dir}"
    return 1
  fi

  if ! tar -xzf "${archive_path}" -C "${tmp_dir}"; then
    rm -rf "${tmp_dir}"
    return 1
  fi

  bundle_path="$(find "${tmp_dir}" -mindepth 1 -maxdepth 3 -type d -name 'Clawbrowser.app' | head -n 1)"
  if [[ -z "${bundle_path}" ]]; then
    rm -rf "${tmp_dir}"
    return 1
  fi

  rm -rf "${INSTALL_ROOT}/Clawbrowser.app"
  cp -R "${bundle_path}" "${INSTALL_ROOT}/Clawbrowser.app"
  rm -rf "${tmp_dir}"
  return 0
}

plugin_install_notice() {
  local message="$1"
  log "WARNING: ${message}"
  log "INFO: Continuing without this plugin step."
  log "INFO: Fallback: use clawbrowser in container mode for servers/no physical display, then rerun with a full release bundle if plugin assets are required."
}

stage_plugin_binaries() {
  local target_dir="$1"

  mkdir -p "${target_dir}/bin"
  cp -L "${INSTALL_ROOT}/bin/clawctl" "${target_dir}/clawctl" 2>/dev/null || true
  cp -L "${INSTALL_ROOT}/bin/clawbrowser" "${target_dir}/clawbrowser" 2>/dev/null || true
  cp -L "${INSTALL_ROOT}/bin/clawbrowser-mcp" "${target_dir}/clawbrowser-mcp" 2>/dev/null || true
  cp -L "${INSTALL_ROOT}/bin/clawctl" "${target_dir}/bin/clawctl" 2>/dev/null || true
  cp -L "${INSTALL_ROOT}/bin/clawbrowser" "${target_dir}/bin/clawbrowser" 2>/dev/null || true
  cp -L "${INSTALL_ROOT}/bin/clawbrowser-mcp" "${target_dir}/bin/clawbrowser-mcp" 2>/dev/null || true
  chmod +x \
    "${target_dir}/clawctl" \
    "${target_dir}/clawbrowser" \
    "${target_dir}/clawbrowser-mcp" \
    "${target_dir}/bin/clawctl" \
    "${target_dir}/bin/clawbrowser" \
    "${target_dir}/bin/clawbrowser-mcp" \
    2>/dev/null || true
}

install_codex_plugin() {
  local source_dir="${SOURCE_ROOT}"
  local target_dir="${CODEX_PLUGINS_ROOT}/clawbrowser"
  local plugin_manifest="${source_dir}/plugins/.codex-plugin/plugin.json"
  local resolved_target_dir

  if [[ ! -f "${plugin_manifest}" ]]; then
    log "INFO: Codex plugin manifest missing at ${plugin_manifest}; skipping plugin copy for this run."
    return 0
  fi

  log "Installing Codex plugin into ${target_dir}"
  if ! mkdir -p "${CODEX_PLUGINS_ROOT}" 2>/dev/null; then
    plugin_install_notice "Unable to create Codex plugins root at ${CODEX_PLUGINS_ROOT}"
    return 0
  fi

  if [[ -e "${target_dir}" ]] && ! rm -rf "${target_dir}"; then
    plugin_install_notice "Unable to replace existing Codex plugin path at ${target_dir}"
    return 0
  fi

  if ! mkdir -p "${target_dir}/.codex-plugin" 2>/dev/null; then
    plugin_install_notice "Unable to create Codex plugin path at ${target_dir}"
    return 0
  fi

  if ! cp -RL "${source_dir}/plugins/.codex-plugin/." "${target_dir}/.codex-plugin/" 2>/dev/null; then
    plugin_install_notice "Failed to copy Codex plugin manifest from ${source_dir}/plugins/.codex-plugin to ${target_dir}/.codex-plugin"
    return 0
  fi

  cp -L "${source_dir}/.mcp.json" "${target_dir}/.mcp.json" 2>/dev/null || true
  cp -L "${source_dir}/SKILL.md" "${target_dir}/SKILL.md" 2>/dev/null || true
  stage_plugin_binaries "${target_dir}"

  resolved_target_dir="$(absolute_path "${target_dir}")"
  python3 - "${target_dir}/.mcp.json" "${resolved_target_dir}/bin/clawctl" "${resolved_target_dir}" <<'PY'
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
command = sys.argv[2]
cwd = sys.argv[3]

payload = json.loads(path.read_text(encoding="utf-8"))
mcp_servers = payload.get("mcpServers")
if not isinstance(mcp_servers, dict):
    raise SystemExit(f"Expected {path} to contain an 'mcpServers' object")

clawbrowser = mcp_servers.get("clawbrowser")
if not isinstance(clawbrowser, dict):
    raise SystemExit(f"Expected {path} to contain a 'clawbrowser' server entry")

clawbrowser["command"] = command
clawbrowser["args"] = ["mcp"]
clawbrowser["cwd"] = cwd
mcp_servers["clawbrowser"] = clawbrowser
path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
PY
}

install_claude_plugin() {
  local source_dir="${SOURCE_ROOT}"
  local target_dir="${CLAUDE_PLUGINS_ROOT}/clawbrowser"
  local plugin_manifest="${source_dir}/plugins/.claude-plugin/plugin.json"

  if [[ ! -f "${plugin_manifest}" ]]; then
    log "INFO: Claude plugin manifest missing at ${plugin_manifest}; skipping plugin copy for this run."
    return 0
  fi

  log "Installing Claude plugin into ${target_dir}"
  if ! mkdir -p "${CLAUDE_PLUGINS_ROOT}" 2>/dev/null; then
    plugin_install_notice "Unable to create Claude plugins root at ${CLAUDE_PLUGINS_ROOT}"
    return 0
  fi

  if [[ -e "${target_dir}" ]] && ! rm -rf "${target_dir}"; then
    plugin_install_notice "Unable to replace existing Claude plugin path at ${target_dir}"
    return 0
  fi

  if ! mkdir -p "${target_dir}" 2>/dev/null; then
    plugin_install_notice "Unable to create Claude plugin path at ${target_dir}"
    return 0
  fi

  if ! cp -RL "${source_dir}/plugins/.claude-plugin/." "${target_dir}/" 2>/dev/null; then
    plugin_install_notice "Failed to copy Claude plugin manifest from ${source_dir}/plugins/.claude-plugin to ${target_dir}"
    return 0
  fi

  cp -L "${source_dir}/AGENTS.md" "${target_dir}/AGENTS.md" 2>/dev/null || true
  cp -L "${source_dir}/SKILL.md" "${target_dir}/SKILL.md" 2>/dev/null || true
  stage_plugin_binaries "${target_dir}"

  log "Claude plugin installed: ${target_dir}"
}

write_codex_marketplace() {
  local marketplace_file="${AGENTS_PLUGINS_ROOT}/marketplace.json"
  local plugin_dir="${CODEX_PLUGINS_ROOT}/clawbrowser"

  if [[ ! -d "${plugin_dir}" ]]; then
    log "INFO: Codex plugin directory missing at ${plugin_dir}; skipping marketplace metadata update."
    return 0
  fi

  if ! mkdir -p "${AGENTS_PLUGINS_ROOT}" 2>/dev/null; then
    plugin_install_notice "Unable to create Codex marketplace directory at ${AGENTS_PLUGINS_ROOT}"
    return 0
  fi

  python3 - "${marketplace_file}" "${plugin_dir}" <<'PY'
import json
import os
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
plugin_dir = pathlib.Path(sys.argv[2])

desired_plugins = [
    {
        "name": "clawbrowser",
        "description": "Managed Clawbrowser runtime for agent tasks: lifecycle and identity via CLI/MCP, page automation via CDP, fingerprint/proxy-backed sessions, and browser-managed config.json reuse.",
        "source": {
            "source": "local",
            "path": os.path.relpath(plugin_dir, start=path.parent),
        },
        "policy": {
            "installation": "INSTALLED_BY_DEFAULT",
            "authentication": "NONE",
        },
        "category": "Productivity",
    },
]

payload = {}
if path.exists():
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise SystemExit(f"Invalid JSON in {path}: {exc}") from exc
    if not isinstance(payload, dict):
        raise SystemExit(f"Expected {path} to contain a JSON object")

plugins = payload.get("plugins")
if plugins is None:
    plugins = []
elif not isinstance(plugins, list):
    raise SystemExit(f"Expected {path} to contain a 'plugins' array")

merged_plugins = []
desired_by_name = {plugin["name"]: plugin for plugin in desired_plugins}
merged_names = set()

for plugin in plugins:
    plugin_name = plugin.get("name") if isinstance(plugin, dict) else None
    desired_plugin = desired_by_name.get(plugin_name or "")
    if desired_plugin:
        if plugin_name in merged_names:
            continue

        merged = dict(plugin)
        merged.update(desired_plugin)

        merged_source = dict(plugin.get("source") or {})
        merged_source.update(desired_plugin["source"])
        merged["source"] = merged_source

        merged_policy = dict(plugin.get("policy") or {})
        merged_policy.update(desired_plugin["policy"])
        merged["policy"] = merged_policy

        merged_plugins.append(merged)
        merged_names.add(plugin_name)
        continue

    merged_plugins.append(plugin)

for desired_plugin in desired_plugins:
    if desired_plugin["name"] not in merged_names:
        merged_plugins.append(desired_plugin)

payload["plugins"] = merged_plugins
payload.setdefault("name", "clawbrowser-marketplace")

interface = payload.get("interface")
if not isinstance(interface, dict):
    interface = {}
payload["interface"] = interface
interface.setdefault("displayName", "Clawbrowser")

tmp_path = path.with_suffix(path.suffix + ".tmp")
tmp_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
tmp_path.replace(path)
PY
  log "Wrote Codex marketplace metadata: ${marketplace_file}"
}

prepare_runtime_root() {
  local preserved_app_dir=""
  local preserved_app_path_file=""
  local preserve_tmp=""
  local path

  if [[ -e "${INSTALL_ROOT}/Clawbrowser.app" || -f "${INSTALL_ROOT}/app_bundle_path" ]]; then
    preserve_tmp="$(mktemp -d)"
    if [[ -e "${INSTALL_ROOT}/Clawbrowser.app" ]]; then
      cp -R "${INSTALL_ROOT}/Clawbrowser.app" "${preserve_tmp}/Clawbrowser.app" 2>/dev/null || true
      preserved_app_dir="${preserve_tmp}/Clawbrowser.app"
    fi
    if [[ -f "${INSTALL_ROOT}/app_bundle_path" ]]; then
      cp "${INSTALL_ROOT}/app_bundle_path" "${preserve_tmp}/app_bundle_path" 2>/dev/null || true
      preserved_app_path_file="${preserve_tmp}/app_bundle_path"
    fi
  fi

  log "Installing runtime files into ${INSTALL_ROOT}"
  rm -rf "${INSTALL_ROOT}"
  mkdir -p "${INSTALL_ROOT}"

  mkdir -p "${INSTALL_ROOT}/bin" "${INSTALL_ROOT}/scripts"
  cp -R "${SOURCE_ROOT}/bin/." "${INSTALL_ROOT}/bin/"

  for path in \
    "AGENTS.md" \
    "SKILL.md" \
    "INSTALL.md" \
    "README.md" \
    ".mcp.json" \
    "package.json" \
    "scripts/install.sh"
  do
    if [[ -f "${SOURCE_ROOT}/${path}" ]]; then
      mkdir -p "${INSTALL_ROOT}/$(dirname "${path}")"
      cp -L "${SOURCE_ROOT}/${path}" "${INSTALL_ROOT}/${path}"
    fi
  done

  find "${INSTALL_ROOT}" -type d -name "__pycache__" -prune -exec rm -rf {} + 2>/dev/null || true

  if [[ -n "${preserved_app_dir}" && -e "${preserved_app_dir}" ]]; then
    rm -rf "${INSTALL_ROOT}/Clawbrowser.app"
    cp -R "${preserved_app_dir}" "${INSTALL_ROOT}/Clawbrowser.app"
  fi
  if [[ -n "${preserved_app_path_file}" && -f "${preserved_app_path_file}" ]]; then
    cp "${preserved_app_path_file}" "${INSTALL_ROOT}/app_bundle_path"
  fi
  if [[ -n "${preserve_tmp}" ]]; then
    rm -rf "${preserve_tmp}"
  fi

  chmod +x \
    "${INSTALL_ROOT}/bin/clawbrowser-install.js" \
    "${INSTALL_ROOT}/bin/clawctl" \
    "${INSTALL_ROOT}/bin/clawbrowser" \
    "${INSTALL_ROOT}/bin/clawbrowser-mcp" \
    "${INSTALL_ROOT}/scripts/install.sh" \
    2>/dev/null || true
}

link_launchers() {
  mkdir -p "${INSTALL_BIN}"
  ln -sfn "${INSTALL_ROOT}/bin/clawctl" "${INSTALL_BIN}/clawctl"
  ln -sfn "${INSTALL_ROOT}/bin/clawbrowser" "${INSTALL_BIN}/clawbrowser"
  ln -sfn "${INSTALL_ROOT}/bin/clawbrowser-mcp" "${INSTALL_BIN}/clawbrowser-mcp"
}

link_app_bundle() {
  local source_bundle="$1"
  local target_bundle="${INSTALL_ROOT}/Clawbrowser.app"

  if [[ "${source_bundle}" == "${target_bundle}" ]]; then
    return 0
  fi

  ln -sfn "${source_bundle}" "${target_bundle}"
  printf '%s\n' "${source_bundle}" > "${INSTALL_ROOT}/app_bundle_path"
}

register_gemini_extension() {
  cp -L "${SOURCE_ROOT}/GEMINI.md" "${INSTALL_ROOT}/GEMINI.md" 2>/dev/null || true
  cp -L "${SOURCE_ROOT}/gemini-extension.json" "${INSTALL_ROOT}/gemini-extension.json" 2>/dev/null || true
  mkdir -p "${GEMINI_EXTENSIONS_ROOT}"
  ln -sfn "${INSTALL_ROOT}" "${GEMINI_EXTENSIONS_ROOT}/clawbrowser"
  log "Registered Gemini CLI extension: ${GEMINI_EXTENSIONS_ROOT}/clawbrowser"
}

install_hermes_plugin() {
  local source_dir="${SOURCE_ROOT}/plugins/.hermes-plugin"
  local target_dir="${HERMES_PLUGINS_ROOT}/clawbrowser"

  if [[ ! -d "${source_dir}" ]]; then
    log "Hermes plugin source not found at ${source_dir}; skipping"
    return 0
  fi

  log "Installing Hermes plugin into ${target_dir}"
  if ! mkdir -p "${HERMES_PLUGINS_ROOT}" 2>/dev/null; then
    plugin_install_notice "Unable to create Hermes plugins root at ${HERMES_PLUGINS_ROOT}"
    return 0
  fi

  if [[ -e "${target_dir}" ]] && ! rm -rf "${target_dir}"; then
    plugin_install_notice "Unable to replace existing Hermes plugin path at ${target_dir}"
    return 0
  fi

  # -L dereferences symlinks so the destination has real file content
  # (e.g. SKILL.md which is a symlink to the repo-root canonical copy).
  if ! cp -RL "${source_dir}" "${target_dir}" 2>/dev/null; then
    plugin_install_notice "Failed to copy Hermes plugin from ${source_dir} to ${target_dir}"
    return 0
  fi
  find "${target_dir}" -type d -name "__pycache__" -prune -exec rm -rf {} + 2>/dev/null || true

  stage_plugin_binaries "${target_dir}"

  log "Hermes plugin installed: ${target_dir}"
}

install_openclaw_plugin() {
  local source_dir="${SOURCE_ROOT}/plugins/.openclaw-plugin"
  local target_dir="${INSTALL_ROOT}/.openclaw-plugin"

  if [[ ! -d "${source_dir}" ]]; then
    log "OpenClaw plugin source not found at ${source_dir}; skipping"
    return 0
  fi

  log "Installing OpenClaw plugin into ${target_dir}"
  if [[ -e "${target_dir}" ]] && ! rm -rf "${target_dir}"; then
    plugin_install_notice "Unable to replace existing OpenClaw plugin path at ${target_dir}"
    return 0
  fi

  if ! cp -RL "${source_dir}" "${target_dir}" 2>/dev/null; then
    plugin_install_notice "Failed to copy OpenClaw plugin from ${source_dir} to ${target_dir}"
    return 0
  fi
  chmod +x "${target_dir}/init.sh" 2>/dev/null || true

  if OPENCLAW_PLUGIN_MODE=install "${target_dir}/init.sh" >/dev/null 2>&1; then
    log "Initialized OpenClaw plugin config"
  else
    plugin_install_notice "OpenClaw plugin init script failed: ${target_dir}/init.sh"
  fi

  log "OpenClaw plugin installed: ${target_dir}"
}

enable_hermes_plugin() {
  local hermes_config="${HOME}/.hermes/config.yaml"
  local resolved_install_bin
  local mcp_command

  resolved_install_bin="$(absolute_path "${INSTALL_BIN}")"
  mcp_command="${resolved_install_bin}/clawctl"

  if [[ ! -x "${mcp_command}" ]]; then
    mcp_command="$(command -v clawctl 2>/dev/null || printf '%s\n' "clawctl")"
  fi

  if [[ "${mcp_command}" != /* && "${mcp_command}" == */* ]]; then
    mcp_command="$(absolute_path "${mcp_command}")"
  fi

  if [[ ! -f "${hermes_config}" ]]; then
    mkdir -p "$(dirname "${hermes_config}")"
    python3 - "${hermes_config}" "${mcp_command}" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
mcp_command = sys.argv[2]
path.write_text(
    "plugins:\n"
    "  enabled:\n"
    "    - clawbrowser\n"
    "\n"
    "mcp_servers:\n"
    "  clawbrowser:\n"
    f"    command: {mcp_command!r}\n"
    "    args: ['mcp']\n",
    encoding="utf-8",
)
PY
    log "Created Hermes config with clawbrowser enabled: ${hermes_config}"
    return 0
  fi

  python3 - "${hermes_config}" "${mcp_command}" <<'PY'
import sys

config_path = sys.argv[1]
mcp_command = sys.argv[2]

with open(config_path, "r") as f:
    content = f.read()

lines = content.rstrip("\n").split("\n") if content else []


def indentation(line):
    return len(line) - len(line.lstrip(" "))


def is_blank_or_comment(line):
    stripped = line.strip()
    return not stripped or stripped.startswith("#")


def is_key(line, key, indent):
    return indentation(line) == indent and line.strip().split("#", 1)[0].strip() == f"{key}:"


def top_level_block(key):
    start = None
    for index, line in enumerate(lines):
        if is_key(line, key, 0):
            start = index
            break
    if start is None:
        return None, None

    end = len(lines)
    for index in range(start + 1, len(lines)):
        if not is_blank_or_comment(lines[index]) and indentation(lines[index]) == 0:
            end = index
            break
    return start, end


plugins_index = None
for index, line in enumerate(lines):
    if is_key(line, "plugins", 0):
        plugins_index = index
        break

if plugins_index is None:
    if lines:
        lines.append("")
    lines.extend(["plugins:", "  enabled:", "    - clawbrowser"])
else:
    block_end = len(lines)
    for index in range(plugins_index + 1, len(lines)):
        if not is_blank_or_comment(lines[index]) and indentation(lines[index]) == 0:
            block_end = index
            break

    enabled_index = None
    for index in range(plugins_index + 1, block_end):
        if is_key(lines[index], "enabled", 2):
            enabled_index = index
            break

    if enabled_index is None:
        lines[plugins_index + 1:plugins_index + 1] = ["  enabled:", "    - clawbrowser"]
    else:
        enabled_block_end = block_end
        for index in range(enabled_index + 1, block_end):
            if not is_blank_or_comment(lines[index]) and indentation(lines[index]) <= 2:
                enabled_block_end = index
                break

        enabled_items = {
            line.strip()[2:].strip()
            for line in lines[enabled_index + 1:enabled_block_end]
            if indentation(line) >= 4 and line.strip().startswith("- ")
        }
        if "clawbrowser" not in enabled_items:
            lines.insert(enabled_block_end, "    - clawbrowser")

mcp_index, mcp_end = top_level_block("mcp_servers")
if mcp_index is None:
    if lines and lines[-1].strip():
        lines.append("")
    lines.extend([
        "mcp_servers:",
        "  clawbrowser:",
        f"    command: {mcp_command!r}",
        "    args: ['mcp']",
    ])
else:
    claw_index = None
    for index in range(mcp_index + 1, mcp_end):
        if is_key(lines[index], "clawbrowser", 2):
            claw_index = index
            break

    if claw_index is None:
        lines[mcp_end:mcp_end] = [
            "  clawbrowser:",
            f"    command: {mcp_command!r}",
            "    args: ['mcp']",
        ]
    else:
        claw_end = mcp_end
        for index in range(claw_index + 1, mcp_end):
            if not is_blank_or_comment(lines[index]) and indentation(lines[index]) <= 2:
                claw_end = index
                break

        command_index = None
        args_index = None
        for index in range(claw_index + 1, claw_end):
            if indentation(lines[index]) == 4 and lines[index].strip().split("#", 1)[0].strip().startswith("command:"):
                command_index = index
            if indentation(lines[index]) == 4 and lines[index].strip().split("#", 1)[0].strip().startswith("args:"):
                args_index = index

        command_line = f"    command: {mcp_command!r}"
        args_line = "    args: ['mcp']"
        if command_index is None:
            lines.insert(claw_index + 1, command_line)
            command_index = claw_index + 1
            if args_index is not None and args_index >= command_index:
                args_index += 1
        else:
            lines[command_index] = command_line
        if args_index is None:
            lines.insert(command_index + 1, args_line)
        else:
            lines[args_index] = args_line

with open(config_path, "w") as f:
    f.write("\n".join(lines) + "\n")
PY

  log "Enabled clawbrowser and MCP server in Hermes config: ${hermes_config}"
}

docker_image_available() {
  local image="$1"
  if ! command -v docker >/dev/null 2>&1; then
    return 1
  fi

  docker image inspect "${image}" >/dev/null 2>&1 || docker manifest inspect "${image}" >/dev/null 2>&1
}

main() {
  parse_args "$@"
  resolve_target
  require_command bash
  require_command mktemp
  require_command python3

  ensure_source_root
  prepare_runtime_root
  link_launchers

  native_bundle=""
  if native_bundle="$(resolve_app_bundle 2>/dev/null)"; then
    log "Found macOS app bundle: ${native_bundle}"
  fi

  if [[ -n "${native_bundle}" ]]; then
    link_app_bundle "${native_bundle}"
  elif download_native_app_bundle; then
    log "Installed native app bundle into ${INSTALL_ROOT}/Clawbrowser.app"
  else
    if docker_image_available "${RUNTIME_IMAGE}"; then
      log "No macOS app bundle found, using existing Docker image"
    else
      log "No macOS app bundle found, using Docker runtime image ${RUNTIME_IMAGE}"
    fi
  fi

  if [[ "${TARGET}" == "codex" ]]; then
    install_codex_plugin
    write_codex_marketplace
  fi

  if [[ "${TARGET}" == "gemini" ]]; then
    register_gemini_extension
  fi

  if [[ "${TARGET}" == "claude" ]]; then
    install_claude_plugin
  fi

  if [[ "${TARGET}" == "hermes" ]]; then
    install_hermes_plugin
    enable_hermes_plugin
  fi

  if [[ "${TARGET}" == "openclaw" ]]; then
    install_openclaw_plugin
  fi

  if [[ "${TARGET}" == "codex" ]]; then
    log "Codex target selected: the runtime files, Codex plugin copy, and marketplace metadata are installed."
  fi

  if [[ "${TARGET}" == "claude" ]]; then
    log "Claude target selected: the runtime files and Claude plugin copy are installed."
  fi

  if [[ "${TARGET}" == "hermes" ]]; then
    log "Hermes target selected: the runtime files, Hermes plugin, and MCP config are installed."
  fi

  if [[ "${TARGET}" == "openclaw" ]]; then
    log "OpenClaw target selected: the runtime files and OpenClaw plugin are installed."
  fi

  log "Clawbrowser installed."
  log "Commands:"
  log "  clawctl"
  log "  clawbrowser"
  log "  clawbrowser-mcp"
  log ""
  log "Next steps for agents:"
  log "  1. Start managed sessions with:"
  log "     clawctl start --session work --url https://example.com --json"
  log "  2. Get CDP with:"
  log "     clawctl endpoint --session work --json"
  log "  3. Use CDP for page automation."
  log "  4. Use clawbrowser://verify only for fingerprint/proxy/geo checks."
  log "  5. Use clawctl rotate --session work --json for a fresh identity."
  log "  6. Do not launch browser binaries directly for agent tasks."
  log "     On macOS, Clawbrowser.app may be the native runtime used under the hood."
  log ""
  log "If API key is missing:"
  log "  Get it from https://app.clawbrowser.ai."
  log "  The launcher/browser stores it in the browser-managed config.json."
  log "  Resolve config paths before writing; do not use unresolved shell-expression paths with file/write tools; they may create literal workspace paths instead of the real config file."
  log "  Do not store it in MCP config, agent config, shell startup files, or logs."
}

main "$@"
