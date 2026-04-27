---
name: clawbrowser
description: Install and operate Clawbrowser as an agent-only managed browser runtime. Lifecycle and identity come from the CLI/MCP layer, page automation comes from CDP, and managed sessions are expected to run in fingerprint/proxy mode. See AGENTS.md for the full contract.
---

# Clawbrowser Skill

Full contract: [AGENTS.md](./AGENTS.md)

## Short Contract

- Lifecycle and identity live in the CLI/MCP layer: `clawbrowser start` or MCP `start_session` create or reattach managed sessions; `clawbrowser endpoint` or MCP `endpoint_session` returns the CDP handle.
- Managed sessions for agent tasks are expected to run in fingerprint/proxy mode. If `clawbrowser://verify` reports fingerprint mode inactive, the session was not launched correctly.
- `--session <name>` is the handle for a managed profile or identity. Reuse the same name to reattach; use a new name for a separate browser instance. Keep a session-to-endpoint mapping when you work with more than one profile.
- `clawbrowser rotate --session <name>` is the public fresh-identity path. Use `clawbrowser://verify` only when identity, proxy, or geo proof matters, after rotate/regenerate, or when debugging browser-quality issues.
- Browser-managed `config.json` is the source of truth for saved auth. If it is missing, ask once for the real API key from https://app.clawbrowser.ai and use `clawbrowser://auth` for manual reauth.
- Cleanup and inspection live in the CLI/MCP layer too: `clawbrowser status`, `clawbrowser list`, and `clawbrowser stop`.

Legacy compatibility note: `openclaw` exists only for historical bootstrap support in the installer. Do not use it in agent workflows or quick references.

## Normal Flow

```bash
clawbrowser start --session work -- https://example.com
clawbrowser endpoint --session work
clawbrowser rotate --session work -- clawbrowser://verify
clawbrowser status --session work
clawbrowser list --session work
clawbrowser stop --session work
```

## Runtime Modes

- **Host mode** — desktop/macOS or Linux with a display. Config: `${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser/config.json`.
- **Container mode** — VPS, server, SSH-only, or no physical display. OCI image `docker.io/clawbrowser/clawbrowser:latest` (Docker or a Docker-compatible OCI CLI; set `CLAWBROWSER_DOCKER_BIN` for non-Docker launchers). This runs full Clawbrowser with a virtual Linux display and exposes CDP; it is not Chrome headless mode. Config in the container at `/home/clawbrowser/.config/clawbrowser/config.json`, persisted in the `clawbrowser-config` named volume.

Default to container mode when there is no display. `auto` mode also falls back native→container on startup failure.

## Install

- Prefer the repo-local marketplace or the `scripts/install.sh` bootstrap.
- If you are installing, use the installer `auto` target unless you need a specific target such as `hermes`, `claude`, `codex`, `gemini`, or legacy `openclaw`.
- In container mode, also pull and run the container image. See `INSTALL.md` for the exact commands and config paths.
