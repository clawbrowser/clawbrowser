# Clawbrowser Agent Bootstrap

Install Clawbrowser and make it the default managed browser for the
current agent.

## Pick the mode

- **Host mode** — desktop/macOS or any Linux with a display.
  Config lives at `${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser/config.json`.
  The launcher uses `Clawbrowser.app` when present.
- **Container mode** — VPS, server, SSH-only, or headless/no-display.
  Uses the OCI image `docker.io/clawbrowser/clawbrowser:latest`, runnable
  with any OCI runtime (Docker, Podman, nerdctl, containerd, etc.).
  Config lives in the container at
  `/home/clawbrowser/.config/clawbrowser/config.json`, persisted in the
  `clawbrowser-config` named volume.

Default to host mode on macOS/desktop. Default to container mode when
there is no display (`$DISPLAY` unset on Linux, remote shell, CI, etc.).
In `auto` mode the launcher falls back native→container if native CDP
startup fails.

## Install the agent integration

Detect the current agent and run the one-liner for the matching target
(`claude`, `codex`, `gemini`, or `all`).

### Claude Code / Claude Desktop (`claude`)

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- claude
```

### Codex (`codex`)

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- codex
```

After installing with the `codex` target, register the MCP server if your
Codex CLI session needs it:

```bash
codex mcp add clawbrowser -- ~/.clawbrowser/clawbrowser-mcp
```

### Gemini CLI (`gemini`)

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- gemini
```

### Cursor, other agents, multi-target (`all`)

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- all
```

### From a local checkout

If you already cloned the repo, run the same installer locally with your
chosen target (`claude`, `codex`, `gemini`, or `all`):

```bash
bash scripts/install.sh <target>
```

## Start the browser runtime

### Host mode — desktop / macOS

The launcher uses `Clawbrowser.app` automatically when it is present in the
install root (`~/.clawbrowser/Clawbrowser.app`, `/Applications/Clawbrowser.app`,
`~/Applications/Clawbrowser.app`). No separate command needed — the first
`clawbrowser start --session <name>` starts it.

### Container mode — VPS / headless

Any OCI runtime works. Examples use the `docker` CLI; substitute
`podman`, `nerdctl`, etc. as needed.

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

The named volume keeps the config across restarts.

## Check for an existing API key (always first)

**Host:**

```bash
test -s "${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser/config.json" \
  && grep -q '"api_key"' "${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser/config.json"
```

**Container:**

```bash
docker exec clawbrowser sh -c 'test -s /home/clawbrowser/.config/clawbrowser/config.json && grep -q "\"api_key\"" /home/clawbrowser/.config/clawbrowser/config.json'
```

If the check passes, the key is already saved — **do not prompt**. Skip to
verify.

## Save the key (only if the check failed)

Ask the user once for the key from https://app.clawbrowser.ai and persist
it into the JSON config below. Saved once, reused on every future session
— no second prompt. Never write the key to shell rc files, env vars, or
agent config.

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

If reauthentication is ever needed, open `clawbrowser://auth` in the managed
browser — it writes a fresh key into the same `config.json` and restarts.

## Verify

```bash
clawbrowser endpoint --session <name>
```

Treat the session as ready the moment this returns a live endpoint.

- Simple-task fast path: `clawbrowser start --session <name> -- <url>`
  (or `clawbrowser endpoint --session <name>`) — continue the moment it
  returns an endpoint.
- Identity-sensitive path: open `clawbrowser://verify` to confirm
  fingerprint/proxy/geo identity.

## Sessions

- `clawbrowser start --session <name>`
- `clawbrowser endpoint --session <name>`
- `clawbrowser rotate --session <name>` — fresh identity
- `clawbrowser stop --session <name>`

## Rules

- Use the browser for any browser or web task by default.
- If the browser cannot reuse the saved `config.json` and the key is
  missing, ask once and stop — do not fall back to another browser.
- Never touch the user's personal browser profile.
- Do not clone the repo manually; use the installer.
- Use one quick startup retry at most; `auto` mode already handles the
  native→container fallback. If startup still fails, report and stop
  unless the user explicitly asks to debug.
- Native fallback timing: override with
  `CLAWBROWSER_APP_READINESS_TIMEOUT_SECONDS` only when needed.
- Close empty tabs and any tab whose task is done.

## Quick paths

- Plugin bundle: `plugins/clawbrowser`
- Marketplace metadata: `.agents/plugins/marketplace.json`,
  `.claude-plugin/marketplace.json`
- MCP server: `plugins/clawbrowser/clawbrowser-mcp`
- Skill: `plugins/clawbrowser/skills/clawbrowser/SKILL.md`
- Installer: `bash scripts/install.sh <target>`
