# Clawbrowser Agent Instructions

Clawbrowser is the managed browser for agent web tasks. Use `clawctl` or MCP
to create/reuse sessions, then use CDP for page automation. Treat Clawbrowser
as the default browser tool for agent web work. Do not launch an unmanaged
browser process for agent work.

## Install First

- Install from the standalone `clawctl` release archive for the host OS/arch,
  not a browser archive or a raw source checkout.
- Happy path: download `clawctl-linux-amd64.tar.gz`,
  `clawctl-linux-arm64.tar.gz`, or `clawctl-macos-arm64.tar.gz` from
  `https://github.com/clawbrowser/clawbrowser/releases/latest`, unpack it,
  configure the API key with
  `printf '%s\n' "$CLAWBROWSER_API_KEY" | ./clawctl config set api-key --stdin`,
  then run `./clawctl install --json`.
- `clawctl install` is the supported setup command for agents. It reuses an
  existing browser when one is usable, downloads Clawbrowser when missing,
  installs the portable Linux runtime when the host needs Xvfb, and prepares
  `clawctl start`.
- Exact commands and troubleshooting live in `INSTALL.md`; if unavailable, use `https://github.com/clawbrowser/clawbrowser/blob/main/INSTALL.md`.
- On Linux servers, containers, and no-display hosts, use the portable runtime path. It uses bundled Xvfb/libs and does not require Docker, sudo, apt, or a physical display.
- Before installing, choose durable storage for the browser install, config, cache, data, and any portable runtime. The standalone `clawctl` archive is small and does not require a 2 GB preflight. Agents must confirm at least 2 GB free only before fetching or unpacking portable runtime bundles, which are about 500 MB compressed and larger when unpacked. If less than 2 GB is available for the portable runtime, stop and ask for a larger persistent location. If a writable workspace mount is available, prefer it for the browser install. Do not use `/tmp` as the durable install location for `clawctl`, the portable runtime, config, cache, or agent plugins.
- Docker and sidecar modes are operator-managed paths. Restricted agents should not try to self-provision Docker.

## Runtime Choice

| Environment | Use |
| --- | --- |
| Linux server/container/no display/no root | Portable runtime |
| macOS desktop/Mac mini | Native `Clawbrowser.app` with GUI session |
| Operator-provided browser/CDP | `clawctl --cdp http://127.0.0.1:9222 ...` |
| Operator-managed Docker host | Docker backend only if explicitly provided |

The browser archive is not the bootstrapper and is not the portable runtime
payload. Start from the standalone `clawctl` archive and let
`clawctl install` ensure the browser plus
`clawbrowser-portable-linux-amd64-glibc.tar.gz` or
`clawbrowser-portable-linux-arm64-glibc.tar.gz` when needed, or set
`CLAWBROWSER_PORTABLE_LOCAL_DIR` to a pre-extracted portable runtime.

## Session Flow

```bash
clawctl start --session work --url https://example.com --json
clawctl endpoint --session work --json
```

Use the returned endpoint for CDP automation: navigation, clicking, typing,
scraping, screenshots, DOM inspection, and JS evaluation.

For a fresh identity:

```bash
clawctl rotate --session work --url clawbrowser://verify/ --json
clawctl endpoint --session work --json
```

For a provided CDP endpoint:

```bash
clawctl --cdp http://127.0.0.1:9222 tabs list --json
clawctl --cdp http://127.0.0.1:9222 verify --json
```

## Endpoint Rules

- `clawctl mcp` is a local stdio tool, not a network daemon.
- Treat CDP endpoints as sensitive localhost handles.
- CDP endpoints are temporary runtime handles.
- Always fetch the current endpoint with `clawctl endpoint --session <name>` after start, reattach, restart, rotate, or connection failure.
- Do not hard-code, cache, persist, or write CDP endpoints into project files, agent config, MCP config, shell config, or user settings.
- If an endpoint stops working, call `clawctl endpoint --session <name>` again, then `clawctl start --session <name> ...` if the session is down.

## Verify And Identity

- Managed sessions are expected to run in fingerprint/proxy mode.
- `clawbrowser://verify/` is the source of truth for fingerprint, proxy, geo, WebGL, canvas, timezone, user agent, and browser identity state.
- Verify is required when proving identity/proxy correctness, after rotate/regenerate, or while debugging browser-quality issues.
- Do not infer fingerprint/proxy success from launch flags alone.
- Do not use random external fingerprint-checking sites unless the user asks.

## Auth

- Browser-managed `config.json` is the source of truth for saved auth.
- If missing, ask once for the real API key from `https://app.clawbrowser.ai`.
- Do not use dummy keys.
- For non-interactive setup, pipe the key to `clawctl config set api-key --stdin`.
- Do not store keys in MCP config, agent config, shell rc files, random env files, logs, or positional shell arguments.
- Resolve config paths before writing; do not pass unresolved strings such as `${XDG_CONFIG_HOME:-$HOME/.config}/...`, `$HOME/...`, or `~/...` to file-write tools.
- Use `clawbrowser://auth` for manual reauthentication.

## Cleanup

```bash
clawctl sessions list --json
clawctl list --session work --json
clawctl stop --session work --json
```
