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
   clawctl proxy-traffic --json
   clawctl endpoint --profile work --json
   clawctl verify --profile work --json
   ```

   If `proxy-traffic` reports `state: "exhausted"`, stop proxy-backed browser
   work until the user tops up traffic in the dashboard.

No Docker, sudo, apt, manual portable runtime download, or physical display is
required for Linux server/container installs.

### Windows

Use PowerShell on 64-bit Windows. Start with the standalone `clawctl` archive;
do not start from the browser zip. This path requires the selected GitHub
release to include `clawctl-win-amd64.zip` and `clawbrowser-win-amd64.zip`.

```powershell
$archive = "clawctl-win-amd64.zip"
$url = "https://github.com/clawbrowser/clawbrowser/releases/latest/download/$archive"

Invoke-WebRequest -Uri $url -OutFile $archive
Expand-Archive -Force $archive .
Set-Location .\clawctl-win-amd64

.\clawctl.exe install --json
.\clawctl.exe config set api-key
.\clawctl.exe start --profile work --url clawbrowser://verify/ --json
.\clawctl.exe endpoint --profile work --json
.\clawctl.exe verify --profile work --json
```

#### Windows Agent Install

`clawctl install` downloads the matching `clawbrowser-win-amd64.zip` browser
payload when no usable Windows install exists. If that payload contains
`setup.exe`, `clawctl` runs it with the normal `/silent` installer switch and
Windows may show an administrator approval prompt.

Human users can double-click `setup.exe` and get the interactive Clawbrowser
Setup window with Install, Cancel, install status, and Close controls. AI
agents should run setup through `clawctl install` or invoke the extracted
installer directly with:

```powershell
.\setup.exe /silent
```

This bypasses the interactive window and installs Clawbrowser without launching
browser UI after setup.

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
| `clawctl-linux-amd64.tar.gz`, `clawctl-linux-arm64.tar.gz`, `clawctl-macos-arm64.tar.gz`, `clawctl-win-amd64.zip` | Standalone bootstrapper archives. Start here. |
| `clawbrowser-linux-amd64.tar.gz`, `clawbrowser-linux-arm64.tar.gz`, `clawbrowser-macos-arm64.tar.gz`, `clawbrowser-win-amd64.zip` | Browser payload archives. `clawctl install` downloads one when no usable browser exists. Windows payloads may contain `setup.exe` or `Clawbrowser\clawbrowser.exe`; there is no release-owned public plugin-spec install surface. `clawctl install` and `clawctl start` own setup and launch. |
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

### Remote Viewing

If the user asks for a stream, live view, remote browser, or remote control,
open the requested page and start a remote session:

```bash
clawctl open --profile work https://example.com --json
clawctl remote --profile work --wait=false --json
```

Share only the returned `viewer_url` with the requesting user. Treat it as a
sensitive temporary control link. If multiple tabs are open, pass
`--target-url-contains` or `--target-title-contains`.

For an already-running browser/CDP sidecar:

```bash
clawctl --cdp http://127.0.0.1:9222 tabs list --json
clawctl --cdp http://127.0.0.1:9222 verify --json
```

## Runtime Modes

- **Portable Linux runtime** - default for Linux servers, no-display hosts, and
  restricted containers. No Docker required.
- **macOS native app** - default on macOS. Requires a GUI desktop context.
- **Windows native install** - default on 64-bit Windows. Uses the Windows
  browser payload and may require an administrator approval prompt when
  `setup.exe` is present.
- **Docker backend** - optional, operator-managed infrastructure only.
- **Existing CDP endpoint** - explicit `clawctl --cdp http://127.0.0.1:9222 ...`
  mode for sidecars or externally provisioned browsers.

Use Docker only when the operator intentionally provides it. Restricted agents
should use portable mode or a provided CDP endpoint.
