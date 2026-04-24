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
TARGET="${CLAWBROWSER_TARGET:-all}"
SOURCE_ARCHIVE_URL="${CLAWBROWSER_SOURCE_ARCHIVE_URL:-}"
RESOLVED_RELEASE_TAG=""

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
  bash scripts/install.sh [all|codex|claude|gemini|hermes]
  bash scripts/install.sh --target <all|codex|claude|gemini|hermes>
  curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- <target>

Environment overrides:
  CLAWBROWSER_INSTALL_ROOT   Bundle install root, default: ~/.clawbrowser
  CLAWBROWSER_INSTALL_BIN    Command install directory, default: ~/.local/bin
  CLAWBROWSER_CLAUDE_PLUGINS_ROOT  Claude plugin directory, default: ~/.claude/plugins
  CLAWBROWSER_GEMINI_EXTENSIONS_ROOT  Gemini extension directory, default: ~/.gemini/extensions
  CLAWBROWSER_HERMES_PLUGINS_ROOT  Hermes plugin directory, default: ~/.hermes/plugins
  CLAWBROWSER_RUNTIME_IMAGE  Docker image used at runtime when no native app bundle exists, default: docker.io/clawbrowser/clawbrowser:latest
  CLAWBROWSER_RELEASE_REF    Release ref or tag, default: latest
  CLAWBROWSER_APP_PATH       Optional macOS Clawbrowser.app path or executable
  CLAWBROWSER_SOURCE_ARCHIVE_URL  Optional archive override; defaults to the tagged release archive for CLAWBROWSER_RELEASE_REF

The browser-managed config.json is reused automatically once saved. If the
saved config is missing, the launcher prompts once and writes the key into
the browser-managed config. Use clawbrowser://auth only for manual browser
setup or reauthentication. Do not put the API key in agent-side config files
or shell startup scripts; let the browser manage its own config storage.
EOF
}

is_source_root_ready() {
  [[ -x "${SOURCE_ROOT}/bin/clawbrowser" ]] &&
    [[ -x "${SOURCE_ROOT}/bin/clawbrowser-mcp" ]] &&
    [[ -f "${SOURCE_ROOT}/.claude-plugin/plugin.json" ]] &&
    [[ -f "${SOURCE_ROOT}/.codex-plugin/plugin.json" ]] &&
    [[ -f "${SOURCE_ROOT}/.hermes-plugin/plugin.yaml" ]] &&
    [[ -f "${SOURCE_ROOT}/gemini-extension.json" ]] &&
    [[ -f "${SOURCE_ROOT}/AGENTS.md" ]] &&
    [[ -f "${SOURCE_ROOT}/SKILL.md" ]] &&
    [[ -f "${SOURCE_ROOT}/scripts/install.sh" ]]
}

resolve_release_tag() {
  if [[ -n "${RESOLVED_RELEASE_TAG}" ]]; then
    printf '%s\n' "${RESOLVED_RELEASE_TAG}"
    return 0
  fi

  if [[ "${RELEASE_REF}" == "latest" ]]; then
    require_command python3
    RESOLVED_RELEASE_TAG="$(
      python3 - <<'PY'
import json
from urllib.request import Request, urlopen

request = Request(
    "https://api.github.com/repos/clawbrowser/clawbrowser/releases/latest",
    headers={
        "Accept": "application/vnd.github+json",
        "User-Agent": "clawbrowser-install",
    },
)
with urlopen(request, timeout=15) as response:
    payload = json.load(response)

tag_name = payload.get("tag_name")
if not tag_name:
    raise SystemExit("missing tag_name in GitHub releases/latest response")

print(tag_name)
PY
    )"
  else
    RESOLVED_RELEASE_TAG="${RELEASE_REF}"
  fi

  printf '%s\n' "${RESOLVED_RELEASE_TAG}"
}

release_source_archive_url() {
  if [[ -n "${SOURCE_ARCHIVE_URL}" ]]; then
    printf '%s\n' "${SOURCE_ARCHIVE_URL}"
    return 0
  fi

  printf 'https://codeload.github.com/%s/tar.gz/refs/tags/%s' \
    "${REPOSITORY}" "$(resolve_release_tag)"
}

bootstrap_source_root() {
  local tmp_dir extracted_root
  local archive_url

  require_command curl
  require_command tar
  require_command mktemp

  tmp_dir="$(mktemp -d)"
  archive_url="$(release_source_archive_url)"
  log "Source checkout not found locally; downloading ${archive_url}"
  curl -fsSL "${archive_url}" | tar -xzf - -C "${tmp_dir}"

  extracted_root="$(find "${tmp_dir}" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
  if [[ -z "${extracted_root}" ]]; then
    die "Unable to locate the extracted source archive"
  fi

  SOURCE_ROOT="${extracted_root}"
}

ensure_source_root() {
  if ! is_source_root_ready; then
    bootstrap_source_root
  fi
}

normalize_target() {
  case "${1}" in
    all|codex|claude|gemini|hermes) printf '%s\n' "${1}" ;;
    *)
      die "Unknown target: ${1}"
      ;;
  esac
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
      all|codex|claude|gemini|hermes)
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

host_arch() {
  case "$(uname -m)" in
    arm64|aarch64) printf '%s\n' arm64 ;;
    x86_64|amd64) printf '%s\n' x64 ;;
    *)
      die "Unsupported architecture: $(uname -m)"
      ;;
  esac
}

asset_name() {
  case "$(host_arch)" in
    arm64) printf '%s\n' clawbrowser-linux-arm64.tar.gz ;;
    x64) printf '%s\n' clawbrowser-linux-x64.tar.gz ;;
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
  log "INFO: Fallback: use clawbrowser in headless/container mode, then rerun with a full release bundle if plugin assets are required."
}

ensure_plugin_bundle_dir() {
  local bundle_dir="$1"
  if [[ -d "${bundle_dir}" ]]; then
    return 0
  fi

  if mkdir -p "${bundle_dir}" 2>/dev/null; then
    return 0
  fi

  plugin_install_notice "Unable to create plugin directory at ${bundle_dir}"
  return 1
}

find_plugin_source_dir() {
  local candidate

  for candidate in \
    "${SOURCE_ROOT}/plugins/clawbrowser" \
    "${SOURCE_ROOT}/clawbrowser/plugins/clawbrowser" \
    "${SOURCE_ROOT}/bundle/plugins/clawbrowser"
  do
    if [[ -d "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  candidate="$(find "${SOURCE_ROOT}" -mindepth 2 -maxdepth 5 -type d -path '*/plugins/clawbrowser' 2>/dev/null | head -n 1 || true)"
  if [[ -n "${candidate}" ]]; then
    printf '%s\n' "${candidate}"
    return 0
  fi

  return 1
}

install_codex_plugin() {
  local source_dir="${INSTALL_ROOT}/plugins/clawbrowser"
  local target_dir="${CODEX_PLUGINS_ROOT}/clawbrowser"
  local plugin_manifest="${source_dir}/.codex-plugin/plugin.json"

  if [[ ! -d "${source_dir}" ]]; then
    log "INFO: Codex plugin source missing at ${source_dir}; skipping plugin copy for this run."
    log "INFO: Fallback plan: rerun with a release asset that includes plugin files, or continue with launcher-only install."
    return 0
  fi

  if [[ ! -f "${plugin_manifest}" ]]; then
    log "INFO: Codex plugin manifest missing at ${plugin_manifest}; skipping plugin copy for this run."
    log "INFO: Fallback plan: rerun with a release asset that includes plugin files, or continue with launcher-only install."
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

  # -L dereferences symlinks so the destination has real file content
  # (e.g. SKILL.md which is a symlink to the repo-root canonical copy).
  if ! cp -RL "${source_dir}" "${target_dir}" 2>/dev/null; then
    plugin_install_notice "Failed to copy Codex plugin from ${source_dir} to ${target_dir}"
    return 0
  fi
}

install_claude_plugin() {
  local source_dir="${INSTALL_ROOT}/plugins/clawbrowser"
  local target_dir="${CLAUDE_PLUGINS_ROOT}/clawbrowser"
  local plugin_manifest="${source_dir}/.claude-plugin/plugin.json"

  if [[ ! -f "${plugin_manifest}" ]]; then
    log "INFO: Claude plugin manifest missing at ${plugin_manifest}; skipping plugin copy for this run."
    log "INFO: Fallback plan: rerun with a release asset that includes plugin files, or continue with launcher-only install."
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

  if ! cp -RL "${source_dir}/.claude-plugin/." "${target_dir}/" 2>/dev/null; then
    plugin_install_notice "Failed to copy Claude plugin manifest from ${source_dir}/.claude-plugin to ${target_dir}"
    return 0
  fi

  mkdir -p "${target_dir}/bin" 2>/dev/null || true
  cp -L "${INSTALL_ROOT}/AGENTS.md" "${target_dir}/AGENTS.md" 2>/dev/null || true
  cp -L "${INSTALL_ROOT}/SKILL.md" "${target_dir}/SKILL.md" 2>/dev/null || true
  cp "${INSTALL_ROOT}/bin/clawbrowser" "${target_dir}/clawbrowser" 2>/dev/null || true
  cp "${INSTALL_ROOT}/bin/clawbrowser-mcp" "${target_dir}/clawbrowser-mcp" 2>/dev/null || true
  cp "${INSTALL_ROOT}/bin/clawbrowser" "${target_dir}/bin/clawbrowser" 2>/dev/null || true
  cp "${INSTALL_ROOT}/bin/clawbrowser-mcp" "${target_dir}/bin/clawbrowser-mcp" 2>/dev/null || true
  chmod +x \
    "${target_dir}/clawbrowser" \
    "${target_dir}/clawbrowser-mcp" \
    "${target_dir}/bin/clawbrowser" \
    "${target_dir}/bin/clawbrowser-mcp" \
    2>/dev/null || true

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
        "description": "Agent-only browser runtime with browser-managed config.json reuse, CDP sessions, rotation, and default browser routing.",
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

copy_bundle() {
  local preserved_app_dir=""
  local preserved_app_path_file=""
  local preserve_tmp=""

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

  log "Installing shared bundle into ${INSTALL_ROOT}"
  rm -rf "${INSTALL_ROOT}"
  mkdir -p "${INSTALL_ROOT}"
  cp -R "${SOURCE_ROOT}/." "${INSTALL_ROOT}/"

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
    "${INSTALL_ROOT}/bin/clawbrowser" \
    "${INSTALL_ROOT}/bin/clawbrowser-mcp" \
    "${INSTALL_ROOT}/bin/openclaw-plugin-init" \
    "${INSTALL_ROOT}/.openclaw-plugin/init.sh" \
    "${INSTALL_ROOT}/scripts/install.sh" \
    "${INSTALL_ROOT}/scripts/clawbrowser_launcher_test.sh" \
    2>/dev/null || true
}

materialize_compat_bundle() {
  local bundle_dir="${INSTALL_ROOT}/plugins/clawbrowser"
  local fallback_source=""

  if ! ensure_plugin_bundle_dir "${bundle_dir}"; then
    return 1
  fi

  if [[ ! -e "${bundle_dir}/.codex-plugin/plugin.json" ]]; then
    fallback_source="$(find_plugin_source_dir || true)"
    if [[ -n "${fallback_source}" ]] && [[ "${fallback_source}" != "${bundle_dir}" ]]; then
      log "Using fallback plugin source at ${fallback_source}"
      if ! cp -RL "${fallback_source}/." "${bundle_dir}/" 2>/dev/null; then
        log "WARNING: Failed to copy fallback plugin source from ${fallback_source}; continuing with compatibility links."
      fi
    else
      log "INFO: plugins/clawbrowser was not found in source; creating compatibility plugin layout."
    fi
  fi

  ln -sfn ../../.claude-plugin "${bundle_dir}/.claude-plugin"
  ln -sfn ../../.codex-plugin "${bundle_dir}/.codex-plugin"
  ln -sfn ../../.hermes-plugin "${bundle_dir}/.hermes-plugin"
  ln -sfn ../../.mcp.json "${bundle_dir}/.mcp.json"
  ln -sfn ../../AGENTS.md "${bundle_dir}/AGENTS.md"
  ln -sfn ../../AGENTS.md "${bundle_dir}/CLAUDE.md"
  ln -sfn ../../AGENTS.md "${bundle_dir}/GEMINI.md"
  ln -sfn ../../SKILL.md "${bundle_dir}/SKILL.md"
  ln -sfn ../../bin/clawbrowser "${bundle_dir}/clawbrowser"
  ln -sfn ../../bin/clawbrowser-mcp "${bundle_dir}/clawbrowser-mcp"
  mkdir -p "${bundle_dir}/bin"
  ln -sfn ../../../bin/clawbrowser "${bundle_dir}/bin/clawbrowser"
  ln -sfn ../../../bin/clawbrowser-mcp "${bundle_dir}/bin/clawbrowser-mcp"
  ln -sfn ../../gemini-extension.json "${bundle_dir}/gemini-extension.json"

  if [[ ! -e "${bundle_dir}/.codex-plugin/plugin.json" ]]; then
    touch "${bundle_dir}/.stub" 2>/dev/null || true
    plugin_install_notice "Compatibility plugin metadata still missing under ${bundle_dir}"
    return 1
  fi

  return 0
}

materialize_openclaw_plugin_bundle() {
  local source_dir="${SOURCE_ROOT}/.openclaw-plugin"
  local target_dir="${INSTALL_ROOT}/.openclaw-plugin"

  if [[ ! -d "${source_dir}" ]]; then
    log "INFO: OpenClaw scaffold source not found at ${source_dir}; skipping"
    return 0
  fi

  if ! mkdir -p "${INSTALL_ROOT}/plugins" 2>/dev/null; then
    plugin_install_notice "Unable to create plugin directory at ${INSTALL_ROOT}/plugins"
    return 0
  fi

  if [[ -e "${target_dir}" ]] && ! rm -rf "${target_dir}"; then
    plugin_install_notice "Unable to replace existing OpenClaw scaffold at ${target_dir}"
    return 0
  fi

  if ! cp -RL "${source_dir}" "${target_dir}" 2>/dev/null; then
    plugin_install_notice "Failed to copy OpenClaw scaffold from ${source_dir} to ${target_dir}"
    return 0
  fi

  chmod +x "${target_dir}/init.sh" 2>/dev/null || true
  log "Installed OpenClaw scaffold into ${target_dir}"
}

initialize_openclaw_plugin() {
  local init_script="${INSTALL_ROOT}/.openclaw-plugin/init.sh"

  if [[ ! -x "${init_script}" ]]; then
    log "INFO: OpenClaw scaffold init script not found at ${init_script}; skipping bootstrap"
    return 0
  fi

  if OPENCLAW_PLUGIN_MODE=install "${init_script}" >/dev/null 2>&1; then
    log "Initialized OpenClaw plugin bootstrap config"
    return 0
  fi

  plugin_install_notice "OpenClaw scaffold init script failed: ${init_script}"
  return 0
}

link_launchers() {
  mkdir -p "${INSTALL_BIN}"
  ln -sfn "${INSTALL_ROOT}/bin/clawbrowser" "${INSTALL_BIN}/clawbrowser"
  ln -sfn "${INSTALL_ROOT}/bin/clawbrowser-mcp" "${INSTALL_BIN}/clawbrowser-mcp"
  ln -sfn "${INSTALL_ROOT}/bin/openclaw-plugin-init" "${INSTALL_BIN}/openclaw-plugin-init"
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
  mkdir -p "${GEMINI_EXTENSIONS_ROOT}"
  ln -sfn "${INSTALL_ROOT}" "${GEMINI_EXTENSIONS_ROOT}/clawbrowser"
  log "Registered Gemini CLI extension: ${GEMINI_EXTENSIONS_ROOT}/clawbrowser"
}

install_hermes_plugin() {
  local source_dir="${INSTALL_ROOT}/plugins/clawbrowser/.hermes-plugin"
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

  # Copy the clawbrowser and clawbrowser-mcp binaries into the plugin
  # so the tools can locate them even without PATH setup.
  cp "${INSTALL_ROOT}/bin/clawbrowser" "${target_dir}/clawbrowser" 2>/dev/null || true
  cp "${INSTALL_ROOT}/bin/clawbrowser-mcp" "${target_dir}/clawbrowser-mcp" 2>/dev/null || true
  chmod +x "${target_dir}/clawbrowser" "${target_dir}/clawbrowser-mcp" 2>/dev/null || true

  log "Hermes plugin installed: ${target_dir}"
}

enable_hermes_plugin() {
  local hermes_config="${HOME}/.hermes/config.yaml"

  if [[ ! -f "${hermes_config}" ]]; then
    mkdir -p "$(dirname "${hermes_config}")"
    printf 'plugins:\n  enabled:\n    - clawbrowser\n' > "${hermes_config}"
    log "Created Hermes config with clawbrowser enabled: ${hermes_config}"
    return 0
  fi

  python3 - "${hermes_config}" <<'PY'
import sys

config_path = sys.argv[1]

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

with open(config_path, "w") as f:
    f.write("\n".join(lines) + "\n")
PY

  log "Enabled clawbrowser in Hermes config: ${hermes_config}"
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
  require_command bash
  require_command mktemp
  require_command python3

  ensure_source_root
  copy_bundle
  if ! materialize_compat_bundle; then
    log "INFO: Proceeding without a complete compatibility plugin bundle; Codex/Hermes plugin steps may be skipped."
  fi
  materialize_openclaw_plugin_bundle
  initialize_openclaw_plugin
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

  if [[ "${TARGET}" == "all" || "${TARGET}" == "codex" ]]; then
    install_codex_plugin
    write_codex_marketplace
  fi

  if [[ "${TARGET}" == "all" || "${TARGET}" == "gemini" ]]; then
    register_gemini_extension
  fi

  if [[ "${TARGET}" == "all" || "${TARGET}" == "claude" ]]; then
    install_claude_plugin
  fi

  if [[ "${TARGET}" == "all" || "${TARGET}" == "hermes" ]]; then
    install_hermes_plugin
    enable_hermes_plugin
  fi

  if [[ "${TARGET}" == "codex" ]]; then
    log "Codex target selected: the shared bundle, Codex plugin copy, and marketplace metadata are installed."
  fi

  if [[ "${TARGET}" == "claude" ]]; then
    log "Claude target selected: the shared bundle and Claude plugin copy are installed."
  fi

  log "Installed commands:"
  log "  ${INSTALL_BIN}/clawbrowser"
  log "  ${INSTALL_BIN}/clawbrowser-mcp"
  log "  ${INSTALL_BIN}/openclaw-plugin-init"
  log "Install root:"
  log "  ${INSTALL_ROOT}"
  log "Next: the launcher prompts once if needed and writes the key into browser-managed config"
}

main "$@"
