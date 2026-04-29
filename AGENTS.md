# Clawbrowser Agent Instructions

Clawbrowser is the default managed browser for web tasks. Use the CLI/MCP to create managed sessions, then use CDP for page automation. Do not bypass the managed launcher for agent tasks. On macOS, the launcher may use Clawbrowser.app as the native runtime.

For clawctl-specific command palette guidance, also reference the bundled clawctl skill for the active agent integration.

## Install Prerequisite

- Before using Clawbrowser, install the full browser runtime and `clawctl`; installing only an agent skill/plugin is not enough.
- Happy path: download an assembled release archive from `https://github.com/clawbrowser/clawbrowser/releases/latest`, unpack it, then run `./clawctl install --prompt-api-key auto`.
- Exact install commands live in `INSTALL.md`; if the local file is unavailable, use `https://github.com/clawbrowser/clawbrowser/blob/main/INSTALL.md`.
- Do not use `npx` as the primary install path for agent workflows. If you reached the package through `npx` or a raw source checkout, stop and follow `INSTALL.md` to install from the release archive instead.
- On Linux servers/containers/no-display environments, default to the portable runtime path from `INSTALL.md` (bundled Xvfb + full headful Clawbrowser). This path does not require Docker CLI/daemon/socket at runtime.
- Docker backend and sidecar flows remain optional/operator-managed infrastructure paths; do not assume restricted agents can self-provision Docker.

## Layer Model

- CLI/MCP are the lifecycle and identity layer.
- CDP is the page automation layer.
- Use `clawctl start` or MCP `start_session` to create or reattach managed sessions.
- Use `clawctl endpoint` or MCP `endpoint_session` to get the CDP endpoint.
- Use CDP for navigation, clicking, typing, scraping, screenshots, DOM inspection, and JS evaluation.
- Do not create unmanaged browser processes.

## CDP Endpoint Handling

- CDP endpoints returned by Clawbrowser are temporary runtime handles.
- Always obtain the current endpoint with `clawctl endpoint --session <name>`.
- Do this after start, reattach, restart, or rotate.
- Do not hard-code, cache, or persist CDP endpoints.
- Do not write CDP endpoints to agent config, plugin config, shell config, project files, or user settings.
- Do not reuse previously observed `ws://127.0.0.1/...` endpoints after restart or rotate.
- If an endpoint stops working, call `clawctl endpoint --session <name>`.

## MCP Security

- `clawctl mcp` is local stdio only, not a network daemon.
- The packaged `clawbrowser-mcp` server remains available for compatibility; it is also local stdio only.
- It exposes lifecycle/session tools and returns the local CDP endpoint; treat that endpoint as sensitive.
- Do not expose CDP on the network or publish the Docker port externally unless you explicitly understand the risk.
- Do not put API keys into MCP config, agent config, shell rc files, or logs.
- Use the official `clawbrowser/clawbrowser` GitHub repository and install command only.

## Session Model

- `--session <name>` is the handle for a managed browser profile or identity.
- Reusing the same session name reattaches to the same managed session unless rotation or regeneration is requested.
- A new session name creates a separate managed browser instance or profile.
- Keep a session-to-endpoint mapping when you work with multiple profiles.

## Fingerprint Mode

- Managed sessions for agent tasks are expected to run in fingerprint/proxy mode.
- Use `clawctl rotate --session <name>` for a fresh identity. `--regenerate` is the launcher flag behind that path.
- Check `clawbrowser://verify/` after launch, after rotate/regenerate, or when fingerprint/proxy/geo correctness matters.
- If verify shows fingerprint mode inactive, treat the session as misconfigured or launched incorrectly.
- Do not report identity or proxy success when fingerprint mode is inactive.

## Verify / Auth

- `clawbrowser://verify/` is the source of truth for fingerprint, proxy, and geo status.
- It is not required for trivial browsing tasks.
- It is required when proving identity or proxy correctness, after rotate/regenerate tests, or while debugging browser-quality issues.
- Look for: no fingerprint-mode warning, fingerprint mode active, proxy check passes when expected, navigator/user-agent/language/timezone/screen checks pass where applicable, and WebGL/canvas checks pass where applicable.
- Some values may remain the same after rotate because of geo constraints or backend generation. Do not require every field to change.
- Use `clawbrowser://auth` for manual reauthentication.
- The launcher also accepts `--verify` as a convenience when no URL is supplied, but prefer the explicit `clawbrowser://verify/` target in agent-facing examples.

## Fingerprint / Proxy Inspection

- When the user asks to check, inspect, compare, verify, or report fingerprints, proxy, geo, WebGL, canvas, timezone, user agent, or browser identity state, open `clawbrowser://verify/` inside the managed Clawbrowser session and inspect it through CDP.
- Use `clawbrowser://verify/` as the default proof source for Clawbrowser fingerprint/proxy state.
- Do not use random external fingerprint-checking sites unless the user explicitly asks.
- Do not infer fingerprint/proxy status from the launch command alone.
- Do not report fingerprint/proxy success until `clawbrowser://verify/` has been inspected through CDP.

## API Key / Auth

- The browser-managed `config.json` is the source of truth for saved auth.
- If it is missing, ask the user once for the real API key from https://app.clawbrowser.ai.
- Do not use dummy keys.
- Do not store the key in shell rc files, MCP config, agent config, random env files, or logs.
- Resolve config paths before writing. Do not pass `${XDG_CONFIG_HOME:-$HOME/.config}/...`, `$HOME/.config/...`, or `~/.config/...` directly to file/write tools; they may create literal workspace paths instead of the real config file.
- Host config: use the resolved absolute path under the current user's config directory; if you need to write it manually, use a shell command or an already-resolved path.
- Docker backend container config: `/home/clawbrowser/.config/clawbrowser/config.json` in the `clawbrowser-config` named volume.

## Common Workflows

A. Start one managed browser and automate through CDP:

```bash
clawctl start --session work --url https://example.com --json
clawctl endpoint --session work --json
```

B. Start at the verify page:

```bash
clawctl start --session work --url clawbrowser://verify/ --json
clawctl endpoint --session work --json
```

C. Reuse a previous session or profile:

```bash
clawctl start --session work --url https://example.com --json
```

D. Fresh identity for an existing session:

```bash
clawctl rotate --session work --json
```

E. Fresh identity at a chosen URL:

```bash
clawctl rotate --session work --url clawbrowser://verify/ --json
```

Equivalent low-level launcher form:

```bash
clawbrowser start --session work -- --regenerate clawbrowser://verify/
```

F. Two independent browser instances:

```bash
clawctl start --session profile-a --url clawbrowser://verify/ --json
clawctl start --session profile-b --url clawbrowser://verify/ --json
clawctl endpoint --session profile-a --json
clawctl endpoint --session profile-b --json
```

G. Geo or profile constraints:

```bash
clawctl start --session work --json -- --country=US --connection-type=wifi https://example.com
```

These are pass-through browser arguments after `--`; they are not separate launcher options.

H. Missing API key:

- Host mode uses the resolved absolute path under the current user's config directory; resolve it before writing.
- Docker backend container mode uses `/home/clawbrowser/.config/clawbrowser/config.json` in the `clawbrowser-config` named volume.
- Use `clawbrowser://auth` for manual reauthentication.

I. Cleanup:

```bash
clawctl sessions list --json
clawctl list --session work --json
clawctl stop --session work --json
```

`clawctl sessions list` shows live managed sessions. `clawctl list` shows cached browser profiles through the managed launcher.

## Runtime Modes

- **Native macOS** - recommended on macOS. Uses `Clawbrowser.app` with the native WindowServer/GUI session.
- **Portable Linux runtime** - recommended default for Linux VPS/no-display/server/container environments. Runs full headful Clawbrowser with bundled Xvfb and does not require Docker runtime access.
- **Docker backend (optional/operator-managed)** - host-managed Dockerized runtime for operators who intentionally choose it. Config: `/home/clawbrowser/.config/clawbrowser/config.json` in the `clawbrowser-config` named volume.
- **Sidecar integration (optional/operator-managed)** - host-managed sidecar where the agent connects with `clawctl --cdp http://127.0.0.1:9222 ...`.

Default to portable Linux runtime when there is no display. Use Docker backend only when operators intentionally provide Docker infrastructure.
