# Clawbrowser — Install & Verify

Quick install is in [README.md](./README.md). Full contract: [AGENTS.md](./AGENTS.md). This guide covers the exact install commands, saved-key locations, and verify flow.

Clawbrowser release archives ship with `clawctl`, `clawbrowser`, and `clawbrowser-mcp`. Use `clawctl install` as the primary bootstrap path for agent workflows. `clawctl` delegates runtime lifecycle to the `clawbrowser` launcher shipped in this release. Agents should use `clawctl` for browser sessions, tabs, and general browser tasks.

## MCP Security

- `clawctl mcp` and the packaged `clawbrowser-mcp` server are local stdio only, not network daemons.
- It exposes lifecycle/session tools and returns the local CDP endpoint; treat that endpoint as sensitive.
- Do not expose CDP on the network or publish the Docker port externally unless you explicitly understand the risk.
- Do not put API keys into MCP config, agent config, shell rc files, or logs.
- Use the official `clawbrowser/clawbrowser` release archive and the bundled `clawctl` install flow only.

## Install Commands

Clawbrowser ships as a managed browser runtime plus the `clawctl` agent CLI. Use the installer `auto` target if you want it to pick a supported target for you. If you already know which target you want, pass it explicitly.

Recommended:

```bash
./clawctl install --prompt-api-key auto
```

This is the release-artifact bootstrap flow:

```bash
tar -xzf clawbrowser-<platform>.tar.gz
cd clawbrowser-<platform>
./clawctl install --prompt-api-key auto
```

From a local `clawctl` checkout:

```bash
go run ./cmd/clawctl install --prompt-api-key auto
```

Each install run wires up one target plus the shared runtime binaries, installs the `clawctl` binary, and saves the browser API key into the managed config when prompted.
The raw source checkout does not contain generated `bin/clawctl`; release assembly adds it to the tarball. `scripts/install.sh` is an internal release-bundle helper invoked by `clawctl install`, not a standalone remote bootstrap path.

### Targets

Targets:

| Target | Wires up |
| --- | --- |
| `auto` | Detects the matching target automatically |
| `all` | Installs every built-in target |
| `codex` | Codex plugin + `~/.agents/plugins/marketplace.json` |
| `claude-code` | Claude Code plugin bundle |
| `gemini` | Gemini CLI extension |
| `hermes` | Hermes plugin + MCP config in `~/.hermes/config.yaml` |
| `openclaw` | OpenClaw plugin/config integration |

If you need more than one target, rerun the installer once per target.

## Container Mode

Docker or a Docker-compatible OCI CLI can run the published image. This is not Chrome headless mode: it runs full Clawbrowser with a virtual Linux display and exposes CDP. The example uses `docker`; for the launcher set `CLAWBROWSER_DOCKER_BIN` when using a compatible non-Docker CLI:

```bash
docker pull docker.io/clawbrowser/clawbrowser:latest
docker run -d \
  -v clawbrowser-config:/home/clawbrowser/.config/clawbrowser \
  -p 127.0.0.1:9222:9222 \
  --name clawbrowser \
  docker.io/clawbrowser/clawbrowser:latest \
  --remote-debugging-address=127.0.0.1 \
  --remote-debugging-port=9222
```

This binds CDP to localhost only. Do not publish the CDP port on a public interface unless you are deliberately exposing browser automation and understand the risk.

The `clawbrowser-config` named volume keeps the API key across restarts.

## Check For An Existing Key

Use this check to confirm whether `config.json` already contains an `api_key` before you decide to prompt again or run a no-prompt install.

**Host:**

```bash
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser"
python3 - "$CONFIG_DIR/config.json" <<'PY'
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
payload = json.loads(path.read_text())
api_key = payload.get("api_key")
raise SystemExit(0 if isinstance(api_key, str) and api_key.strip() else 1)
PY
```

**Container:**

```bash
docker exec clawbrowser sh -c 'test -s /home/clawbrowser/.config/clawbrowser/config.json && grep -qE "\"api_key\"[[:space:]]*:[[:space:]]*\"[^\"]+\"" /home/clawbrowser/.config/clawbrowser/config.json'
```

If either check passes, skip to [Verify](#verify-and-common-flow).

## Save The Key

When `clawctl install --prompt-api-key` prompts, get the key from [app.clawbrowser.ai](https://app.clawbrowser.ai) and enter it once. The installer writes it into the browser-managed `config.json` and applies restrictive file permissions. Never store it in shell rc files, MCP config, env vars, agent config, or logs.
Resolve config paths before writing. Do not pass `${XDG_CONFIG_HOME:-$HOME/.config}/...` directly to file/write tools; they may create literal workspace paths instead of the real config file.

**Host:**

```bash
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser"
mkdir -p "$CONFIG_DIR"
chmod 700 "$CONFIG_DIR"
python3 - "$CONFIG_DIR/config.json" "$API_KEY" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
key = sys.argv[2].strip()
path.write_text(json.dumps({"api_key": key}, separators=(",", ":")) + "\n")
path.chmod(0o600)
PY
```

**Container** (persists in the named volume):

```bash
docker exec clawbrowser sh -c '
  mkdir -p /home/clawbrowser/.config/clawbrowser &&
  chmod 700 /home/clawbrowser/.config/clawbrowser &&
  printf "{\"api_key\":\"%s\"}\n" "THE_KEY" > /home/clawbrowser/.config/clawbrowser/config.json &&
  chmod 600 /home/clawbrowser/.config/clawbrowser/config.json
'
```

For manual reauthentication, open `clawbrowser://auth` in the managed browser. It writes a fresh key into the same `config.json` and restarts.

Validate after writing the file:

```bash
python3 - "$CONFIG_DIR/config.json" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
data = json.loads(path.read_text())
key = data.get("api_key", "")
print("config_path", path)
print("exists", path.exists())
print("valid_json", True)
print("keys", list(data.keys()))
print("has_api_key", bool(key))
print("api_key_length", len(key))
print("api_key_preview", (key[:4] + "..." + key[-4:]) if len(key) >= 8 else "<too short>")
PY
```

If Clawbrowser still reports "No API key found":

- verify the resolved config path;
- check file permissions;
- check JSON shape is exactly:
  `{"api_key":"..."}`
- search for accidental literal paths:
  `find "$PWD" "$HOME" -path '*${XDG_CONFIG_HOME*' -o -path '*$HOME*' 2>/dev/null`
- remove accidental literal config paths only after confirming they are wrong.

## Verify And Common Flow

Use `clawctl` for browser sessions, tabs, and page tasks. `verify` is the explicit verify page and the source of truth for fingerprint, proxy, and geo status. Use it when identity or proxy proof matters, after rotate/regenerate tests, or when debugging browser-quality issues. If verify reports fingerprint mode inactive, the session was not launched correctly.

```bash
clawctl start --session <name> --url https://example.com --json
clawctl endpoint --session <name> --json
clawctl observe --session <name> --json
clawctl tabs list --session <name> --json
clawctl verify --session <name> --json
clawctl stop --session <name> --json
```

Use tabs for auth, verify, OAuth, checkout, and internal pages. Use `observe`, `click`, `fill`, `open`, and `wait` for normal page work. Some values may remain the same after rotate because of geo constraints or backend generation. Do not require every field to change.

## Quick Reference

- Primary agent CLI: `clawctl` (delegates to `clawbrowser`)
- Agent CLI binary: `bin/clawctl`
- Launcher runtime: `bin/clawbrowser`
- Compatibility MCP server: `bin/clawbrowser-mcp`
- Direct CLI MCP entrypoint: `clawctl mcp`
- Skills: `SKILL.md` (canonical root skill) plus the target-installed clawctl skill/reference where the agent supports skills
- Agent instructions / full contract: `AGENTS.md`
- Canonical plugin manifests: `plugins/.claude-plugin/plugin.json`, `plugins/.codex-plugin/plugin.json`, `plugins/.hermes-plugin/plugin.yaml`, `plugins/.openclaw-plugin/plugin.json`
- Codex marketplace: `~/.agents/plugins/marketplace.json` (written by the installer)
- Installer wrapper: `scripts/install.sh`
- Hermes MCP config: public docs use `command: "clawctl"` plus `args: ["mcp"]`; the installer may write the resolved user-local command path
