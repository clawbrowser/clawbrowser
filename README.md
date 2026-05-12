# Clawbrowser

Clawbrowser is a managed browser runtime for AI agents.

Full agent contract: [AGENTS.md](./AGENTS.md)

Install details and troubleshooting: [INSTALL.md](./INSTALL.md)

## Install

Use the standalone `clawctl` release archive for the host OS/arch. The browser
archive is not the bootstrapper. `clawctl install` owns browser and portable
runtime download/install. The `clawctl` skill/plugin is not enough by itself;
agents still need the standalone `clawctl` binary and the runtime installed by
`clawctl install`. There is no separate `clawbrowser` skill to install; agent
guidance is provided through `clawctl`.

Agent-specific plugin, MCP, and extension templates are owned by `clawctl` and
materialized by `clawctl install`. This includes enabling Hermes plugins and
Gemini extensions in the agent locations those runtimes actually scan. This
release repository does not publish a root `clawbrowser` skill, root `.mcp.json`,
Claude, Codex, Gemini, Hermes, or OpenClaw plugin specs as a manual installation
surface.

Use a durable filesystem for the browser install, config, cache, data, and any
portable runtime. The standalone `clawctl` archive is small and does not
require a 2 GB preflight. The 2 GB free-space check applies only when fetching
or unpacking portable runtime bundles, which are about 500 MB compressed and
larger when unpacked. If a writable workspace mount is available, prefer it for
the browser install. Do not use `/tmp` as the durable install location for
`clawctl`, the portable runtime, config, cache, or agent plugins.

### Agent Install

Use the copy/paste flow in [INSTALL.md](./INSTALL.md#agent-fast-path). The
agent path is intentionally short. Do not rewrite the script and do not use
`/tmp`; many agent containers mount `/tmp` with `noexec`, which causes
`Permission denied` even after `chmod +x`.

1. Find a durable writable directory outside `/tmp`.
2. Download the standalone `clawctl` archive:
   `curl -fL -o "$archive" "$url" && tar -xzf "$archive"` from inside that
   workdir, not `/tmp`.
3. Install Clawbrowser:
   `clawctl install --json`.
   Do not set `CLAWBROWSER_AGENT_CONFIG` or
   `CLAWBROWSER_AGENT_PLUGINS_DIR` unless you intentionally want a generic
   integration path; those overrides bypass agent-specific auto-detection.
   Preserve the active `HOME` unless it is empty, `/root`, or `/tmp`, because
   local agents such as Gemini discover extensions under their real home.
4. Ask once for the Clawbrowser API key from `https://app.clawbrowser.ai`.
5. Save the API key:
   `printf '%s\n' "$CLAWBROWSER_API_KEY" | clawctl config set api-key --stdin`.
6. Verify:

   ```bash
   clawctl start --profile work --url clawbrowser://verify/ --json
   clawctl endpoint --profile work --json
   clawctl verify --profile work --json
   ```

No Docker, sudo, apt, manual portable runtime download, or physical display is
required for Linux server/container installs.

### macOS

```bash
archive="clawctl-macos-arm64.tar.gz"
url="https://github.com/clawbrowser/clawbrowser/releases/latest/download/${archive}"

curl -fL --retry 3 --retry-delay 2 -o "$archive" "$url"
tar -tzf "$archive" >/dev/null
tar -xzf "$archive"
cd clawctl-macos-arm64

./clawctl install --json
./clawctl config set api-key
./clawctl start --profile work --url clawbrowser://verify/ --json
./clawctl endpoint --profile work --json
./clawctl verify --profile work --json
```

macOS uses `Clawbrowser.app` and a GUI WindowServer desktop context. Xvfb is Linux-only.

## Archive Names

| Archive | Purpose |
| --- | --- |
| `clawctl-linux-amd64.tar.gz`, `clawctl-linux-arm64.tar.gz`, `clawctl-macos-arm64.tar.gz` | Standalone bootstrapper archives. Start here. |
| `clawbrowser-linux-amd64.tar.gz`, `clawbrowser-linux-arm64.tar.gz`, `clawbrowser-macos-arm64.tar.gz` | Browser payload archives. `clawctl install` downloads one when no usable browser exists. There is no release-owned public plugin-spec install surface; `clawctl install` and `clawctl start` own setup and launch. |
| `clawbrowser-portable-linux-amd64-glibc.tar.gz`, `clawbrowser-portable-linux-arm64-glibc.tar.gz` | Portable Linux runtime payload with bundled Xvfb, libs, xkb data, and portable browser binary. Ensured by `clawctl install` when Linux needs portable mode. |

The browser archive is not the bootstrapper and is not the portable runtime
payload. Start with the standalone `clawctl` archive; let `clawctl install`
fetch the browser and portable runtime when the host requires them unless you
are building an offline image.
The portable runtime is
unpacked into the persistent runtime root, defaulting to the launcher cache
root's `runtime` directory. Override it with
`CLAWBROWSER_PORTABLE_RUNTIME_ROOT` when the default cache path is not durable
across restarts.

## Agent Flow

```bash
clawctl install --json
clawctl config set api-key
clawctl start --profile work --url clawbrowser://verify/ --json
clawctl endpoint --profile work --json
clawctl verify --profile work --json
```

Attach your CDP client to the returned endpoint. Re-fetch it after start,
restart, rotate, or failure. Do not persist CDP endpoints in config files.

## Remote Control Streaming

Use Remote Control streaming when the user asks to watch, inspect, or control
the browser remotely. For those requests, start streaming as soon as the target
page is loaded instead of waiting for a separate confirmation. Remote Control
creates a backend Remote Session and prints a `viewer_url`; it does not expose
the local CDP endpoint to the viewer.

The saved Clawbrowser API key is required. Agents should persist it once with
`clawctl config set api-key`, then rely on the saved browser `config.json`.
The production Remote Control API is `https://api.clawbrowser.ai`; if an older
`clawctl` returns an HTML `404` from `app.clawbrowser.ai`, update `clawctl` or
temporarily run with `CLAWBROWSER_API_BASE_URL=https://api.clawbrowser.ai`.

Happy path:

```bash
clawctl start --profile work --url https://example.com --json
clawctl wait --profile work --load --timeout 30s --json
clawctl tabs list --profile work --json
clawctl remote --profile work --target-url-contains example.com --json
```

If the task begins with identity rotation, rotate first, then open the target
page again before starting the stream:

```bash
clawctl rotate --profile work --url clawbrowser://verify/ --verify --json
clawctl open --profile work https://example.com --json
clawctl wait --profile work --load --timeout 30s --json
clawctl remote --profile work --target-url-contains example.com --json
```

`clawctl remote` stays attached while the stream is active. Keep that process
running for as long as the viewer should have access; interrupting it closes the
Remote Session, but leaves the browser profile itself running. Treat the printed
`viewer_url` as a sensitive capability URL because it contains a viewer token.

Target selection matters when the profile has multiple tabs, for example a
`clawbrowser://verify/` tab and a website tab. Prefer
`--target-url-contains <domain-or-path>` or
`--target-title-contains <title-fragment>` so the stream attaches to the page
the user expects.

Common Remote Control failures:

| Symptom | Action |
| --- | --- |
| HTML `404` mentioning the dashboard | Use a current `clawctl` release or set `CLAWBROWSER_API_BASE_URL=https://api.clawbrowser.ai`. |
| JSON `401 missing_auth` or `invalid_auth` | Save a real API key with `clawctl config set api-key` and retry. |
| No matching page target | Run `clawctl tabs list --profile work --json`, open the target page, then retry with a narrower `--target-url-contains` value. |
| CDP endpoint refused or stale | Run `clawctl endpoint --profile work --json`; if the profile is stopped, run `clawctl start --profile work --url <url> --json`. |

For an already-running browser/CDP sidecar:

```bash
clawctl --cdp http://127.0.0.1:9222 tabs list --json
clawctl --cdp http://127.0.0.1:9222 verify --json
```

## Runtime Modes

- **Portable Linux runtime** - default for Linux servers, no-display hosts, and
  restricted containers. No Docker required.
- **macOS native app** - default on macOS. Requires a GUI desktop context.
- **Docker backend** - optional, operator-managed infrastructure only.
- **Existing CDP endpoint** - explicit `clawctl --cdp http://127.0.0.1:9222 ...`
  mode for sidecars or externally provisioned browsers.

Use Docker only when the operator intentionally provides it. Restricted agents
should use portable mode or a provided CDP endpoint.
