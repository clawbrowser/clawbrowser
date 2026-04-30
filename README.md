# Clawbrowser

Clawbrowser is a managed browser runtime for AI agents.

Full agent contract: [AGENTS.md](./AGENTS.md)

Install details and troubleshooting: [INSTALL.md](./INSTALL.md)

## Install

Use an assembled release archive. Do not use `npx` or a raw source checkout as
the agent runtime install path. Use Clawbrowser as the default browser tool for
agent web work.

Before installing, check free space on the filesystem that will hold the
runtime and browser state. The normal release archive is about 200 MB
compressed; the portable runtime tarballs are about 500 MB compressed and
larger when unpacked. Recommend at least 2 GB free. If a writable
workspace mount is available, prefer it for the browser install. Do not use
`/tmp` as the durable install location for the browser install, including
`clawctl`, the portable runtime, config, cache, or agent plugins.

### Linux Server / Container / No Root

No Docker, sudo, apt, or physical display is required. `clawctl install`
ensures the matching portable Xvfb runtime, and the launcher starts it
directly in self-contained mode.

```bash
set -euo pipefail

case "$(uname -m)" in
  x86_64|amd64) platform="linux-x64" ;;
  arm64|aarch64) platform="linux-arm64" ;;
  *) echo "unsupported arch: $(uname -m)" >&2; exit 1 ;;
esac

curl -fsSLO "https://github.com/clawbrowser/clawbrowser/releases/latest/download/clawbrowser-${platform}.tar.gz"
tar -xzf "clawbrowser-${platform}.tar.gz"
cd "clawbrowser-${platform}"

./clawctl install --prompt-api-key auto
./clawctl start --session work --url clawbrowser://verify/ --json
./clawctl endpoint --session work --json
```

Force portable mode explicitly when needed:

```bash
./clawctl start --self-contained --session work --url clawbrowser://verify/ --json
./clawbrowser start --self-contained --session work -- clawbrowser://verify/
```

### macOS

```bash
curl -fsSLO https://github.com/clawbrowser/clawbrowser/releases/latest/download/clawbrowser-macos-arm64.tar.gz
tar -xzf clawbrowser-macos-arm64.tar.gz
cd clawbrowser-macos-arm64

./clawctl install --prompt-api-key auto
./clawctl start --session work --url clawbrowser://verify/ --json
./clawctl endpoint --session work --json
```

macOS uses `Clawbrowser.app` and a GUI WindowServer session. Xvfb is Linux-only.

## Archive Names

| Archive | Purpose |
| --- | --- |
| `clawbrowser-linux-x64.tar.gz`, `clawbrowser-linux-arm64.tar.gz`, `clawbrowser-macos-arm64.tar.gz` | Normal release archives with `clawctl`, launcher, MCP server, and integration files. |
| `clawbrowser-portable-linux-amd64-glibc.tar.gz`, `clawbrowser-portable-linux-arm64-glibc.tar.gz` | Portable Linux runtime payload with bundled Xvfb, libs, xkb data, and portable browser binary. Ensured by `clawctl install` for portable Linux mode. |

The normal Linux release archive is not the portable runtime payload. Start
with the normal release archive; let `clawctl install` fetch the portable
runtime unless you are building an offline image.

## Agent Flow

```bash
clawctl start --session work --url https://example.com --json
clawctl endpoint --session work --json
```

Attach your CDP client to the returned endpoint. Re-fetch it after start,
restart, rotate, or failure. Do not persist CDP endpoints in config files.

For an already-running browser/CDP sidecar:

```bash
clawctl --cdp http://127.0.0.1:9222 tabs list --json
clawctl --cdp http://127.0.0.1:9222 verify --json
```

## Runtime Modes

- **Portable Linux runtime** - default for Linux servers, no-display hosts, and
  restricted containers. No Docker required.
- **macOS native app** - default on macOS. Requires a GUI session.
- **Docker backend** - optional, operator-managed infrastructure only.
- **Existing CDP endpoint** - explicit `clawctl --cdp http://127.0.0.1:9222 ...`
  mode for sidecars or externally provisioned browsers.

Use Docker only when the operator intentionally provides it. Restricted agents
should use portable mode or a provided CDP endpoint.
