# Clawbrowser

Clawbrowser is a managed browser runtime for AI agents. Agents use `clawctl`
to install the browser, launch managed profiles, verify fingerprint/proxy
state, automate pages through CDP, and start Remote Control streaming sessions
for a viewer.

Full agent contract: [AGENTS.md](./AGENTS.md)

Install details: [INSTALL.md](./INSTALL.md)

## Install

Start from the standalone `clawctl` release archive for the current OS/arch.
Do not use the browser archive, a raw source checkout, npm, npx, or a
curl-piped installer as the bootstrap path.

```bash
clawctl install --json
clawctl config set api-key
clawctl start --profile work --url clawbrowser://verify/ --json
clawctl endpoint --profile work --json
clawctl verify --profile work --json
```

`clawctl install` installs or reuses Clawbrowser, installs the portable Linux
runtime when the host needs it, and writes the selected agent integrations.
Let it choose integration paths unless you intentionally need a generic target.

If no saved key exists, ask once for the real API key from
`https://app.clawbrowser.ai`, then save it with `clawctl config set api-key`.
Do not store API keys in shell rc files, env files, MCP config, agent config,
project files, or logs.

## Remote Streaming

Use Remote Control streaming when the user wants to watch, inspect, or control
the browser remotely. Start the stream after the target page is loaded. The
stream prints a `viewer_url`; keep the `clawctl remote` process running for as
long as the viewer should have access.

```bash
clawctl start --profile work --url https://example.com --json
clawctl wait --profile work --load --timeout 30s --json
clawctl tabs list --profile work --json
clawctl remote --profile work --target-url-contains example.com --json
```

The JSON output includes the Remote Session id, the `viewer_url`, the selected
profile, and the streamed target page. Treat `viewer_url` as sensitive because
it contains a viewer token. Do not expose or persist local CDP endpoints.

Use an explicit target selector when the profile has multiple tabs:

```bash
clawctl remote --profile work --target-url-contains good-food.ge --json
clawctl remote --profile work --target-title-contains "Example Domain" --json
```

## Rotate Then Stream

For a fresh identity, rotate first, verify, open the target page, then start
Remote Control streaming on that page.

```bash
clawctl rotate --profile work --url clawbrowser://verify/ --verify --json
clawctl open --profile work https://example.com --json
clawctl wait --profile work --load --timeout 30s --json
clawctl tabs list --profile work --json
clawctl remote --profile work --target-url-contains example.com --json
```

## Agent Rules

- Use `clawctl start`, `open`, `wait`, `tabs list`, and `remote` for streaming
  workflows.
- Re-fetch the endpoint after start, rotate, reattach, or connection failure.
- Use `clawbrowser://verify/` as the source of truth for fingerprint, proxy,
  geo, WebGL, canvas, timezone, user agent, and browser identity state.
- Keep Remote Control attached while streaming. Interrupting `clawctl remote`
  closes the Remote Session but leaves the browser profile running.
- Prefer `--target-url-contains` or `--target-title-contains` over relying on
  whichever tab happens to be current.
- Do not launch unmanaged browsers for agent work.

## Existing CDP Endpoint

For an operator-provided browser/CDP sidecar:

```bash
clawctl --cdp http://127.0.0.1:9222 tabs list --json
clawctl --cdp http://127.0.0.1:9222 verify --json
clawctl --cdp http://127.0.0.1:9222 open https://example.com --json
```

## Release Archives

| Archive | Purpose |
| --- | --- |
| `clawctl-linux-amd64.tar.gz`, `clawctl-linux-arm64.tar.gz`, `clawctl-macos-arm64.tar.gz` | Standalone bootstrapper archives. Start here. |
| `clawbrowser-linux-amd64.tar.gz`, `clawbrowser-linux-arm64.tar.gz`, `clawbrowser-macos-arm64.tar.gz` | Browser payload archives downloaded by `clawctl install` when needed. |
| `clawbrowser-portable-linux-amd64-glibc.tar.gz`, `clawbrowser-portable-linux-arm64-glibc.tar.gz` | Portable Linux runtime payload with bundled Xvfb, libraries, xkb data, and portable browser binary. |

## Runtime Modes

- Portable Linux runtime: default for Linux servers, no-display hosts, and
  restricted containers. No Docker required.
- macOS native app: default on macOS. Requires a GUI desktop context.
- Docker backend: optional operator-managed infrastructure only.
- Existing CDP endpoint: explicit `clawctl --cdp http://127.0.0.1:9222 ...`
  mode for sidecars or externally provisioned browsers.
