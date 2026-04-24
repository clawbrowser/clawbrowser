---
name: clawbrowser
description: Install and operate Clawbrowser as an agent-only browser runtime with managed sessions, a local CDP endpoint, and identity rotation. Use when the user wants browser automation, plugin install help, or a browser that does not touch the personal profile.
---

# Clawbrowser Skill

## Mode

- **Host mode** — desktop/macOS or Linux with a display.
  Config: `${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser/config.json`.
- **Container mode** — VPS, server, SSH-only, headless.
  OCI image `docker.io/clawbrowser/clawbrowser:latest` (Docker or a
  Docker-compatible OCI CLI; set `CLAWBROWSER_DOCKER_BIN` for non-Docker
  launchers).
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

Docker or a Docker-compatible OCI CLI can run the published image. The
example uses Docker syntax; for the launcher set `CLAWBROWSER_DOCKER_BIN`
when using a compatible non-Docker CLI:

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
clawbrowser start --session <name> -- about:blank
clawbrowser endpoint --session <name>
```

Treat the session as ready the moment either command returns a live
endpoint.

## Use

- `clawbrowser start --session <name>`
- `clawbrowser start --session <name> -- <url>`
- `clawbrowser endpoint --session <name>`
- `clawbrowser status --session <name>`
- `clawbrowser rotate --session <name>`
- `clawbrowser stop --session <name>`
- `clawbrowser list --session <name>`

## Daily contract

Use this short contract for normal browser work:

| Need | CLI | MCP/Hermes tool |
| --- | --- | --- |
| `start/open` | `clawbrowser start --session <name> -- <url>` | `start_session` / `clawbrowser_start` with `url` |
| `endpoint` | `clawbrowser endpoint --session <name>` | `endpoint_session` / `clawbrowser_endpoint` |
| `rotate` | `clawbrowser rotate --session <name>` | `rotate_session` / `clawbrowser_rotate` |
| `verify/auth` | `clawbrowser start --session <name> -- clawbrowser://verify` or `clawbrowser start --session <name> -- clawbrowser://auth` | start/open with `clawbrowser://verify` or `clawbrowser://auth` |
| `tabs` | use MCP/Hermes tab tools | `list_tabs`/`close_tabs` or `clawbrowser_list_tabs`/`clawbrowser_close_tabs` |
| `stop` | `clawbrowser stop --session <name>` | only when the user asks to close the session |

The returned endpoint is the local CDP endpoint. Use CDP for advanced
automation, but prefer the short tools above for daily browser tasks.

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
- Open `clawbrowser://verify` inside Clawbrowser when fingerprint/proxy/geo
  identity matters or when debugging browser-quality issues.
- Start and rotate commands use fingerprint mode by default. Do not invent
  extra shell flags unless the user asks for a specific geo/profile option.
- Do not run long flag-by-flag verify/fingerprint debug loops for trivial
  tasks the user did not ask to debug.
- Use one quick startup retry at most; `auto` mode already handles the
  native→container fallback. If startup still fails, report and stop
  unless the user asks to debug.
- MANDATORY: close `about:blank`, empty, and no-longer-needed tabs before
  you finish the task. Do not stop the browser session automatically; stop
  only when the user asks to close it or when performing explicit cleanup.
- Use the MCP `list_tabs` tool to find target IDs, then `close_tabs` with
  `target_ids` for specific tabs or `all_pages=true` for batch cleanup.
- If a native session looks stopped right after startup, re-check the CDP
  endpoint before giving up; the launcher PID can disappear before the
  browser is actually unusable.
- For MCP clients, use `clawbrowser-mcp`.
