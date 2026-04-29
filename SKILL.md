---
name: clawbrowser
description: Install and operate Clawbrowser as an agent-only managed browser runtime. Lifecycle and identity come from the CLI/MCP layer, page automation comes from CDP, and managed sessions are expected to run in fingerprint/proxy mode. See AGENTS.md for the full contract.
---

# Clawbrowser Skill

Full contract: [AGENTS.md](./AGENTS.md)

Reference clawctl skill: use the bundled clawctl skill for the active integration.
Use it for the full clawctl command palette and agent workflow details.
This release ships `clawctl`, the `clawbrowser` launcher, and `clawbrowser-mcp`.

## Short Contract

- Lifecycle and identity live in the CLI/MCP layer: `clawctl start` or MCP `start_session` create or reattach managed sessions; `clawctl endpoint` or MCP `endpoint_session` returns the CDP handle.
- Managed sessions are expected to run in fingerprint/proxy mode. If `clawbrowser://verify/` reports fingerprint mode inactive, the session was not launched correctly.
- `--session <name>` is the handle for a managed profile or identity. Reuse the same name to reattach; use a new name for a separate browser instance. Keep a session-to-endpoint mapping when you work with more than one profile.
- `clawctl rotate --session <name>` is the public fresh-identity path. Use `clawbrowser://verify/` only when identity, proxy, or geo proof matters, after rotate/regenerate, or when debugging browser-quality issues.
- Browser-managed `config.json` is the source of truth for saved auth. If it is missing, ask once for the real API key from https://app.clawbrowser.ai, resolve config paths before writing, and use `clawbrowser://auth` for manual reauth.
- Cleanup and inspection live in the CLI/MCP layer too: `clawctl sessions list`, `clawctl list`, and `clawctl stop`.

## CDP Endpoint Handling

- CDP endpoints returned by Clawbrowser are temporary runtime handles.
- Always obtain the current endpoint with `clawctl endpoint --session <name>`.
- Do this after start, reattach, restart, or rotate.
- Do not hard-code, cache, or persist CDP endpoints.
- Do not write CDP endpoints to agent config, plugin config, shell config, project files, or user settings.
- Do not reuse previously observed `ws://127.0.0.1/...` endpoints after restart or rotate.
- If an endpoint stops working, call `clawctl endpoint --session <name>`.

## Fingerprint / Proxy Inspection

- When the user asks to check, inspect, compare, verify, or report fingerprints, proxy, geo, WebGL, canvas, timezone, user agent, or browser identity state, open `clawbrowser://verify/` inside the managed Clawbrowser session and inspect it through CDP.
- Use `clawbrowser://verify/` as the default proof source for Clawbrowser fingerprint/proxy state.
- Do not use random external fingerprint-checking sites unless the user explicitly asks.
- Do not infer fingerprint/proxy status from the launch command alone.
- Do not report fingerprint/proxy success until `clawbrowser://verify/` has been inspected through CDP.

## MCP Security

- `clawctl mcp` and the packaged `clawbrowser-mcp` server are local stdio only, not network daemons.
- It exposes lifecycle/session tools and returns the local CDP endpoint; treat that endpoint as sensitive.
- Do not expose CDP on the network or publish the Docker port externally unless you explicitly understand the risk.
- Do not put API keys into MCP config, agent config, shell rc files, or logs.
- Use the official `clawbrowser/clawbrowser` GitHub repository and install command only.

## API Key / Auth

- The browser-managed `config.json` is the source of truth for saved auth.
- If it is missing, ask the user once for the real API key from https://app.clawbrowser.ai.
- Do not use dummy keys.
- Do not store the key in shell rc files, MCP config, agent config, random env files, or logs.
- Resolve config paths before writing. Do not pass `${XDG_CONFIG_HOME:-$HOME/.config}/...`, `$HOME/.config/...`, or `~/.config/...` directly to file/write tools; they may create literal workspace paths instead of the real config file.
- Host config: use the resolved absolute path under the current user's config directory; if you need to write it manually, use a shell command or an already-resolved path.
- Docker backend container config: `/home/clawbrowser/.config/clawbrowser/config.json` in the `clawbrowser-config` named volume.

## Normal Flow

```bash
clawctl start --session work --url https://example.com --json
clawctl endpoint --session work --json
clawctl rotate --session work --url clawbrowser://verify/ --json
clawctl sessions list --json
clawctl list --session work --json
clawctl stop --session work --json
```

## Runtime Modes

- **Host mode** — desktop/macOS or Linux with a display. Config lives under the current user's config directory; resolve the absolute path before writing.
- **Portable Linux runtime mode** — recommended default for Linux VPS/server/container/no-display environments. Runs full headful Clawbrowser with bundled Xvfb, without requiring Docker runtime access.
- **Docker backend mode (optional/operator-managed)** — Dockerized runtime for operators who intentionally choose Docker and control host infrastructure. Config in the container at `/home/clawbrowser/.config/clawbrowser/config.json`, persisted in the `clawbrowser-config` named volume.
- **Sidecar mode (optional/operator-managed)** — host-managed sidecar where the agent connects via `clawctl --cdp http://127.0.0.1:9222 ...`. See `INSTALL.md` for exact commands.

Default to portable Linux runtime when there is no display. Use Docker backend only when operators intentionally provide that infrastructure.

## Install

- Before using Clawbrowser, install the full browser runtime and `clawctl`; installing only an integration skill/plugin is not enough.
- Happy path: download an assembled release archive from `https://github.com/clawbrowser/clawbrowser/releases/latest`, unpack it, then run the bundled `./clawctl install --prompt-api-key auto`.
- Install guide URL: `https://github.com/clawbrowser/clawbrowser/blob/main/INSTALL.md`. If a local `INSTALL.md` is present beside this skill, prefer that local copy because it matches the installed release.
- Do not use `npx` as the primary install path for agent workflows. If you reach this package through `npx` or a raw source checkout, stop and follow `INSTALL.md` to install from the release archive instead.
- If you are installing, use the installer `auto` target unless you need a specific target such as `hermes`, `claude`, `codex`, `gemini`, or `openclaw`.
- Use the official `clawbrowser/clawbrowser` GitHub repository and bundled `clawctl install` command only.
- On Linux servers/containers/no-display environments, use the portable runtime path by default (bundled Xvfb + headful browser, no Docker runtime required).
- Docker backend and sidecar paths remain optional/operator-managed for hosts that intentionally provide Docker infrastructure.
