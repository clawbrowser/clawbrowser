# Clawbrowser — Install & Verify

Quick install is in [README.md](./README.md). This doc has the exact
commands for verifying the install, saving the API key, and launching
in container mode.

## Install commands

Clawbrowser ships as a CLI + MCP server, plus optional harness-specific
plugins/extensions. Choose the install path that matches your agent
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
| `codex`  | Codex plugin + `~/.agents/plugins/marketplace.json`       |
| `gemini` | Gemini CLI extension                                      |
| `hermes` | Hermes plugin + enables it in `~/.hermes/config.yaml`     |
| `claude` | Claude Code plugin bundle                                |
| `all`    | Everything above (Cursor and other plugin-capable agents) |

## Container mode (VPS / headless)

Any OCI runtime works (Docker, Podman, nerdctl, containerd). Example
with `docker`:

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
test -s "${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser/config.json" \
  && grep -q '"api_key"' "${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser/config.json"
```

**Container:**

```bash
docker exec clawbrowser sh -c 'test -s /home/clawbrowser/.config/clawbrowser/config.json && grep -q "\"api_key\"" /home/clawbrowser/.config/clawbrowser/config.json'
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
clawbrowser endpoint --session <name>
```

The session is ready the moment this returns a live endpoint. If it
doesn't, one retry is allowed (`auto` mode already falls back
native→container). If it still fails, stop — don't loop into a debug
session unless asked.

For identity checks (fingerprint, proxy, geo), open `clawbrowser://verify`
in the managed browser.

## Quick reference

- CLI: `bin/clawbrowser`
- MCP server: `bin/clawbrowser-mcp`
- Skill: `SKILL.md` (symlinked into each plugin subtree)
- Agent instructions: `AGENTS.md` (CLAUDE.md + GEMINI.md symlink here)
- Canonical plugin manifests: `.claude-plugin/plugin.json`, `.codex-plugin/plugin.json`, `.hermes-plugin/plugin.yaml`
- Codex marketplace: `~/.agents/plugins/marketplace.json` (written by the installer)
