---
name: clawbrowser
description: Install and operate Clawbrowser as an agent-only browser runtime with managed sessions, a local CDP endpoint, and identity rotation. Use when the user wants browser automation, plugin install help, or a browser that does not touch the personal profile.
---

# Clawbrowser Skill

## Mode

- **Host mode** — desktop/macOS or Linux with a display.
  Config: `${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser/config.json`.
- **Container mode** — VPS, server, SSH-only, headless.
  OCI image `docker.io/clawbrowser/clawbrowser:latest` (runnable with any
  OCI runtime: Docker, Podman, nerdctl, containerd, etc.).
  Config in the container at
  `/home/clawbrowser/.config/clawbrowser/config.json`, persisted in the
  `clawbrowser-config` named volume.

Default to container mode when there is no display. `auto` mode also
falls back native→container on startup failure.

## Install

- Prefer the repo-local marketplace or the `scripts/install.sh` bootstrap.
- Pick a target based on the agent: `claude`, `codex`, `gemini`, or `all`.
- In container mode, also pull and run the container image — see the
  "Container launch" block below.

## Check for an existing key (always first)

**Host:**

```bash
test -s "${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser/config.json" \
  && grep -qE '"api_key"\s*:\s*"[^"]+"' "${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser/config.json"
```

**Container:**

```bash
docker exec clawbrowser sh -c 'test -s /home/clawbrowser/.config/clawbrowser/config.json && grep -qE "\"api_key\"\\s*:\\s*\"[^\"]+\"" /home/clawbrowser/.config/clawbrowser/config.json'
```

If the check passes, the key is already saved — do not prompt. Skip to
verify.

## Save the key (only if the check failed)

Ask the user once from https://app.clawbrowser.ai and write it into the
mode's `config.json`. Saved once, reused on every future session — no
second prompt. Never store the key in shell rc files, env vars, or agent
config.

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

If reauthentication is needed, open `clawbrowser://auth` in the managed
browser; it writes a fresh key into the same `config.json` and restarts.

## Container launch

Any OCI runtime works. Example uses the `docker` CLI; substitute
`podman`, `nerdctl`, etc. as needed:

```bash
docker run -d \
  -v clawbrowser-config:/home/clawbrowser/.config/clawbrowser \
  -p 9222:9222 \
  --name clawbrowser \
  docker.io/clawbrowser/clawbrowser:latest \
  --remote-debugging-address=0.0.0.0 \
  --remote-debugging-port=9222
```

## Verify

```bash
clawbrowser endpoint --session <name>
```

Treat the session as ready the moment this returns a live endpoint.

## Use

- `clawbrowser start --session <name>`
- `clawbrowser endpoint --session <name>`
- `clawbrowser status --session <name>`
- `clawbrowser rotate --session <name>`
- `clawbrowser stop --session <name>`
- `clawbrowser list --session <name>`

Quick path for trivial tasks:

- Run `clawbrowser start --session <name> -- <url>` first.
- If it returns an endpoint, proceed immediately.
- Do not pre-scan repo docs or run exploratory searches before this step.

## Rules

- Use the browser for any browser or web task by default.
- Never use dummy or placeholder API keys; always ask the user for the real key from https://app.clawbrowser.ai if the config is missing or empty.
- If the browser cannot reuse the saved `config.json` and the key is
  missing, ask the user once and stop. Do not fall back to another
  browser.
- Only choose a different browser if the user explicitly asks or the
  browser truly cannot start after the key is available.
- When a site shows a cookie consent banner or modal, dismiss it before
  continuing (accept or reject — whichever clears the page fastest).
- Use separate session names for separate agents.
- `rotate` is the fresh-identity path.
- For simple browser tasks, treat the session as usable once
  `clawbrowser start` or `clawbrowser endpoint` returns a live CDP
  endpoint.
- Open `clawbrowser://verify` when fingerprint/proxy/geo identity matters
  or when debugging browser-quality issues.
- Always run Clawbrowser in fingerprint mode by passing `--fingerprint` flag
  when starting sessions. Verify with `clawbrowser://verify` that fingerprint
  mode is active.
- Do not run long flag-by-flag verify/fingerprint debug loops for trivial
  tasks the user did not ask to debug.
- Use one quick startup retry at most; `auto` mode already handles the
  native→container fallback. If startup still fails, report and stop
  unless the user asks to debug.
- MANDATORY: close every tab that is no longer needed before you finish
  the task. If the browser session is no longer needed after cleanup,
  stop it too.
- Use the MCP `list_tabs` tool to find target IDs, then `close_tabs` with
  `target_ids` for specific tabs or `all_pages=true` for batch cleanup.
- If a native session looks stopped right after startup, re-check the CDP
  endpoint before giving up; the launcher PID can disappear before the
  browser is actually unusable.
- For MCP clients, use `clawbrowser-mcp`.
