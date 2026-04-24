# Clawbrowser — Install & Verify

Quick install is in [README.md](./README.md). This doc has the exact
commands for verifying the install, saving the API key, and launching
in container mode.

## Install commands

Clawbrowser ships as a CLI + MCP server, plus optional harness-specific
plugins/extensions. Use `auto` unless you already know the exact agent
harness.

### Shell installer (plugins + CLI)

One-liner:

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- <target>
```

From a local checkout:

```bash
bash scripts/install.sh <target>
```

Targets:

| Target   | Wires up                                                  |
|----------|-----------------------------------------------------------|
| `auto`   | Detects the current agent and installs one matching target |
| `hermes` | Hermes plugin + MCP config in `~/.hermes/config.yaml`     |
| `codex`  | Codex plugin + `~/.agents/plugins/marketplace.json`       |
| `gemini` | Gemini CLI extension                                      |
| `claude` | Claude Code plugin bundle                                |
| `all`    | Everything above; use only for intentional multi-target installs |

The installer also materializes the OpenClaw bootstrap scaffold into
`${CLAWBROWSER_INSTALL_ROOT:-~/.clawbrowser}/.openclaw-plugin`,
runs its idempotent `init.sh` bootstrap hook, and creates
`${CLAWBROWSER_INSTALL_BIN:-~/.local/bin}/openclaw-plugin-init`.

## Container mode (VPS / no physical display)

Docker or a Docker-compatible OCI CLI can run the published image. This is
not Chrome headless mode: it runs full Clawbrowser with a virtual Linux
display and exposes CDP. The example uses `docker`; for the launcher set
`CLAWBROWSER_DOCKER_BIN` when using a compatible non-Docker CLI:

```bash
docker pull docker.io/clawbrowser/clawbrowser:latest
docker run -d \
  -v clawbrowser-config:/home/clawbrowser/.config/clawbrowser \
  -p 9222:9222 \
  --name clawbrowser \
  docker.io/clawbrowser/clawbrowser:latest \
  --remote-debugging-address=0.0.0.0 \
  --remote-debugging-port=9222
```

The `clawbrowser-config` named volume keeps the API key across
restarts.

## Check for an existing key

Always do this first — the launcher reuses a saved key automatically
and should not prompt twice.

**Host:**

```bash
python3 - "${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser/config.json" <<'PY'
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

If either check passes, skip to [Verify](#verify).

## Save the key (only if the check failed)

Get the key from [app.clawbrowser.ai](https://app.clawbrowser.ai).
Ask the user exactly once. Never store it in shell rc files, env vars,
or agent config.

**Host:**

```bash
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser"
mkdir -p "$CONFIG_DIR"
umask 077
printf '{"api_key":"%s"}\n' "THE_KEY" > "$CONFIG_DIR/config.json"
chmod 600 "$CONFIG_DIR/config.json"
```

**Container** (persists in the named volume):

```bash
docker exec clawbrowser sh -c '
  mkdir -p /home/clawbrowser/.config/clawbrowser &&
  printf "{\"api_key\":\"%s\"}\n" "THE_KEY" > /home/clawbrowser/.config/clawbrowser/config.json &&
  chmod 600 /home/clawbrowser/.config/clawbrowser/config.json
'
```

For manual reauthentication, open `clawbrowser://auth` in the managed
browser — it writes a fresh key into the same `config.json` and
restarts.

## Verify

```bash
clawbrowser start --session <name> -- clawbrowser://verify
clawbrowser endpoint --session <name>
```

Install verification means the managed browser opened the verify page and
returned a live CDP endpoint. Do not claim deeper fingerprint/proxy quality
unless the browser itself returns a clear verification result.

## Quick reference

- CLI: `bin/clawbrowser`
- MCP server: `bin/clawbrowser-mcp`
- OpenClaw bootstrap launcher: `bin/openclaw-plugin-init`
- OpenClaw bootstrap scaffold: `.openclaw-plugin/`
- Skill: `SKILL.md` (canonical root skill)
- Agent instructions: `AGENTS.md` (CLAUDE.md + GEMINI.md symlink here)
- Canonical plugin manifests: `.claude-plugin/plugin.json`, `.codex-plugin/plugin.json`, `.hermes-plugin/plugin.yaml`
- Codex marketplace: `~/.agents/plugins/marketplace.json` (written by the installer)
- Hermes MCP config: public docs use `command: "clawbrowser-mcp"`; the
  installer may write the resolved user-local command path.
