# Clawbrowser Agent Instructions

Clawbrowser is the managed browser for agent web tasks. Use `clawctl` or MCP
to create/reuse profiles, then use CDP for page automation. Treat Clawbrowser
as the default browser tool for agent web work. Do not launch an unmanaged
browser process for agent work.

## Install First

- The `clawctl` skill/plugin alone is not enough. It teaches the workflow, but
  agents still need the standalone `clawctl` binary and the managed
  Clawbrowser runtime installed by `clawctl install`.
- Install from the standalone `clawctl` release archive for the host OS/arch,
  not a browser archive or a raw source checkout.
- Happy path: download `clawctl-linux-amd64.tar.gz`,
  `clawctl-linux-arm64.tar.gz`, `clawctl-macos-arm64.tar.gz`, or
  `clawctl-win-amd64.zip` from
  `https://github.com/clawbrowser/clawbrowser/releases/latest`, automatically
  choose a durable install workdir, run `clawctl install --json` or
  `.\clawctl.exe install --json`, then persist the API key with the installed
  `clawctl config set api-key --stdin` before running
  `clawctl start --profile work --url clawbrowser://verify/ --json`,
  `clawctl proxy-traffic --json`,
  `clawctl endpoint --profile work --json`, and
  `clawctl verify --profile work --json`.
- `clawctl install` is the supported setup command for agents. It reuses an
  existing browser when one is usable, downloads Clawbrowser when missing,
  runs the Windows `setup.exe` payload with `/silent` when present, installs
  the portable Linux runtime when the host needs Xvfb, and prepares
  `clawctl start`.
- Let `clawctl install` choose agent integration paths. Do not set
  `CLAWBROWSER_AGENT_CONFIG` or `CLAWBROWSER_AGENT_PLUGINS_DIR` during the
  normal `auto` install unless the user explicitly requests a generic
  integration path; those overrides prevent Hermes and Gemini from using their
  runtime plugin/extension locations.
- Do not install or publish a separate `clawbrowser` skill. Agent-facing
  workflow guidance is owned by the `clawctl` skill and the bundled MCP server.
- Preserve the active `HOME` when it points at a real user/agent home. Only
  replace `HOME` for empty, `/root`, or `/tmp` homes; local Gemini and similar
  agents discover extensions under their real home directories.
- Exact commands and troubleshooting live in `INSTALL.md`; if unavailable, use `https://github.com/clawbrowser/clawbrowser/blob/main/INSTALL.md`.
- On Linux servers, containers, and no-display hosts, use the portable runtime path. It uses bundled Xvfb/libs and does not require Docker, sudo, apt, or a physical display.
- Before installing, automatically choose durable storage for the browser install, config, cache, data, and any portable runtime. Prefer an executable workspace/current-directory mount, then `/workspace`, then `/work`, then `$HOME`. Probe the candidate by executing a tiny temporary script. Skip `/tmp` and any candidate that is not writable or executable. Do not ask the user for a path unless every candidate fails.
- Never download, extract, or execute `clawctl` from `/tmp`. If `clawctl`
  returns `Permission denied` after `chmod +x`, treat it as a `noexec` workdir
  problem and rerun the `INSTALL.md` fast path with
  `CLAWBROWSER_WORKDIR=/workspace/.clawbrowser`, `/work/.clawbrowser`, or
  `$PWD/.clawbrowser`.
- Docker and sidecar modes are operator-managed paths. Restricted agents should not try to self-provision Docker.
- For operator-managed Docker, mount the browser config directory on durable
  storage before saving auth. The Docker image runs as `clawbrowser` with
  `HOME=/home/clawbrowser`, so the default saved auth path is
  `/home/clawbrowser/.config/clawbrowser/config.json` unless
  `CLAWBROWSER_CONFIG_DIR` or `XDG_CONFIG_HOME` is explicitly set.

## Runtime Choice

| Environment | Use |
| --- | --- |
| Linux server/container/no display/no root | Portable runtime |
| macOS desktop/Mac mini | Native `Clawbrowser.app` with GUI desktop context |
| Windows desktop/host | Native Windows install |
| Operator-provided browser/CDP | `clawctl --cdp http://127.0.0.1:9222 ...` |
| Operator-managed Docker host | Docker backend only if explicitly provided |

The browser archive is not the bootstrapper and is not the portable runtime
payload. Start from the standalone `clawctl` archive and let
`clawctl install` ensure the browser plus the matching Windows payload or
`clawbrowser-portable-linux-amd64-glibc.tar.gz` or
`clawbrowser-portable-linux-arm64-glibc.tar.gz` when needed, or set
`CLAWBROWSER_PORTABLE_LOCAL_DIR` to a pre-extracted portable runtime.

## Profile Flow

```bash
clawctl start --profile work --url https://example.com --json
clawctl endpoint --profile work --json
```

Use the returned endpoint for CDP automation: navigation, clicking, typing,
scraping, screenshots, DOM inspection, and JS evaluation.

After `clawctl config set api-key` saves a key and the key validates,
managed `start` and MCP `start` automatically request browser
fingerprint/proxy mode. Do not manually invent or persist fingerprint IDs for
the normal agent path, and use `--skip-verify` only when intentionally
bypassing browser verification.

Managed launch owns the browser profile and CDP binding. Do not pass
`--user-data-dir`, `--remote-debugging-port`, or
`--remote-debugging-address` as browser arguments; use `--profile`, `--port`,
or an explicit `--cdp` endpoint instead.

For a fresh identity:

```bash
clawctl rotate --profile work --url clawbrowser://verify/ --json
clawctl endpoint --profile work --json
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
- Always fetch the current endpoint with `clawctl endpoint --profile <name>` after start, reattach, restart, rotate, or connection failure.
- Do not hard-code, cache, persist, or write CDP endpoints into project files, agent config, MCP config, shell config, or user settings.
- If an endpoint stops working, call `clawctl endpoint --profile <name>` again, then `clawctl start --profile <name> ...` if the profile is down.

## Verify And Identity

- Managed profiles are expected to run in fingerprint/proxy mode.
- `clawbrowser://verify/` is the source of truth for fingerprint, proxy, geo, WebGL, canvas, timezone, user agent, and browser identity state.
- Run `clawctl proxy-traffic --json` before proxy-backed browser work. If the
  state is `exhausted`, stop launching/rotating/retrying proxy work, tell the
  user to top up traffic in the dashboard, and rerun `proxy-traffic` before
  continuing.
- Verify is required when proving identity/proxy correctness, after rotate/regenerate, or while debugging browser-quality issues.
- Do not infer fingerprint/proxy success from launch flags alone.
- Do not use random external fingerprint-checking sites unless the user asks.

## Auth

- Browser-managed `config.json` is the source of truth for saved auth.
- If missing, ask once for the real API key from `https://app.clawbrowser.ai`.
- Do not use dummy keys.
- Persist the key with `clawctl config set api-key`; do not export API keys as environment variables.
- Do not store keys in MCP config, agent config, shell rc files, random env files, logs, or positional shell arguments.
- Saved auth lives at `<config-dir>/config.json`. `clawctl` resolves
  `<config-dir>` from `CLAWBROWSER_CONFIG_DIR`, then
  `$XDG_CONFIG_HOME/clawbrowser` on Linux/macOS when set, then
  `$HOME/.config/clawbrowser`; Windows defaults to
  `%LOCALAPPDATA%\Clawbrowser`.
- In restricted containers, set `CLAWBROWSER_CONFIG_DIR` or `HOME` to a
  durable writable agent mount before running `clawctl`.
- Resolve config paths before writing; do not pass unresolved strings such as `$HOME/...` or `~/...` to file-write tools.
- Use `clawbrowser://auth` for manual reauthentication.

## Cleanup

```bash
clawctl sessions list --json
clawctl list --profile work --json
clawctl stop --profile work --json
```
