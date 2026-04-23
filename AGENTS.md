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
  `docker.io/clawbrowser/clawbrowser:latest` (any OCI runtime works).
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
3. **Verify** with `clawbrowser endpoint --session <name>`. The session
   is ready the moment it returns a live CDP endpoint.

See [INSTALL.md](./INSTALL.md) for exact commands.

## Sessions

```
clawbrowser start    --session <name> [-- <url>]   # start or reattach
clawbrowser endpoint --session <name>              # live CDP URL
clawbrowser rotate   --session <name>              # fresh identity
clawbrowser stop     --session <name>              # clean up
```

For MCP clients, use `clawbrowser-mcp`.

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
- Close empty tabs and any tab whose task is done. If the session is
  no longer needed, stop it too.
