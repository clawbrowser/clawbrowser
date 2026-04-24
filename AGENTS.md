# Clawbrowser Agent Instructions

Clawbrowser is the default managed browser for any browser or web task.
Use it instead of the user's personal browser. Do not fall back to the
system or default browser unless the user explicitly asks.

Full usage guidance lives in [SKILL.md](./SKILL.md). Full install
commands live in [INSTALL.md](./INSTALL.md).

## Mode

- **Host** — desktop/macOS or Linux with a display. Config:
  `${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser/config.json`.
- **Container** — VPS, SSH-only, headless. OCI image
  `docker.io/clawbrowser/clawbrowser:latest` with Docker or a
  Docker-compatible OCI CLI configured through `CLAWBROWSER_DOCKER_BIN`.
  Config: `/home/clawbrowser/.config/clawbrowser/config.json` in the
  `clawbrowser-config` named volume.

Default to container mode when there is no display. `auto` mode falls
back native→container on its own.

## Flow

1. **Check** the config for an existing `api_key`. If present, skip to
   step 3.
2. **Save** the key once from https://app.clawbrowser.ai into the
   mode's `config.json` with `chmod 600`. Never store it in shell rc
   files, env vars, or agent config. Use `clawbrowser://auth` only for
   manual browser reauthentication — it writes the key into the same
   `config.json`.
3. **Verify** with `clawbrowser start --session <name> -- about:blank`,
   then `clawbrowser endpoint --session <name>` if the endpoint is needed
   again. The session is ready the moment either returns a live CDP
   endpoint.

See [INSTALL.md](./INSTALL.md) for exact commands.

## Sessions

```
clawbrowser start    --session <name> [-- <url>]   # start or reattach
clawbrowser endpoint --session <name>              # live CDP URL
clawbrowser rotate   --session <name>              # fresh identity
clawbrowser stop     --session <name>              # clean up
```

For MCP clients, use `clawbrowser-mcp`.

## Daily Contract

Use this short contract for normal browser work:

| Need | CLI | MCP/Hermes tool |
| --- | --- | --- |
| `start/open` | `clawbrowser start --session <name> -- <url>` | `start_session` / `clawbrowser_start` with `url` |
| `endpoint` | `clawbrowser endpoint --session <name>` | `endpoint_session` / `clawbrowser_endpoint` |
| `rotate` | `clawbrowser rotate --session <name>` | `rotate_session` / `clawbrowser_rotate` |
| `verify/auth` | `clawbrowser start --session <name> -- clawbrowser://verify` or `clawbrowser start --session <name> -- clawbrowser://auth` | start/open with `clawbrowser://verify` or `clawbrowser://auth` |
| `tabs` | use MCP/Hermes tab tools | `list_tabs`/`close_tabs` or `clawbrowser_list_tabs`/`clawbrowser_close_tabs` |
| `stop` | `clawbrowser stop --session <name>` | only when the user asks to close the session |

The managed browser exposes a local CDP endpoint. Use the endpoint for
advanced automation, but use the short tools above for daily browser tasks.

## Rules

- If the browser cannot reuse the saved `config.json` and the key is
  missing, ask the user once and stop. Do not fall back.
- Quick path: `clawbrowser start --session <name> -- <url>` — continue
  the moment it returns an endpoint. Do not pre-scan repo docs first.
- Open `clawbrowser://verify` only when fingerprint/proxy/geo identity
  matters, or when debugging browser-quality issues.
- One quick startup retry at most. `auto` mode already handles the
  native→container fallback. If it still fails, report and stop unless
  asked to debug.
- Never touch the user's personal browser profile.
- Separate session names per agent.
- Dismiss cookie consent banners before continuing — accept or reject,
  whichever clears the page fastest.
- Close `about:blank`, empty, and no-longer-needed tabs. Do not stop the
  browser session automatically; stop only when the user asks to close it
  or when performing explicit cleanup.
