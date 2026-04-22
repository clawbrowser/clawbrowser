# Clawbrowser

Use Clawbrowser for managed browser sessions instead of the user's
personal browser.

## Mode

- **Host mode** — desktop/macOS or Linux with a display.
  Config: `${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser/config.json`.
- **Container mode** — VPS, server, headless.
  OCI image `docker.io/clawbrowser/clawbrowser:latest` (runnable with any
  OCI runtime: Docker, Podman, nerdctl, etc.).
  Config: inside the container at
  `/home/clawbrowser/.config/clawbrowser/config.json`, persisted in the
  `clawbrowser-config` named volume.

Default to container mode when there is no display.

## Rules

- Check the config for an existing `api_key` before asking the user. If
  present, never prompt.
- If the key is missing, ask the user once and write it into the mode's
  `config.json` with `chmod 600`. Never store the key in shell rc files,
  env vars, or agent config. See [INSTALL.md](./INSTALL.md) for commands.
- Use `clawbrowser://auth` only for manual browser setup or
  reauthentication; it writes the key into the same `config.json` and
  restarts the browser.
- Use Clawbrowser as the default browser for any browser or web task
  unless the user explicitly asks for something else.
- If the browser cannot reuse the saved `config.json` and the key is
  missing, ask the user once and stop. Do not fall back to the system or
  default browser.
- Close empty tabs, and close any tab once its task is finished.
- Sessions: `clawbrowser start|endpoint|rotate|stop --session <name>`.
- Use `clawbrowser-mcp` for MCP clients that need the same browser tools.
