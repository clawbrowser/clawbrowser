# Clawbrowser

Clawbrowser is a managed browser runtime for AI agents.

Full agent contract: [AGENTS.md](./AGENTS.md)

Install details and troubleshooting: [INSTALL.md](./INSTALL.md)

## Install

Use an assembled release archive. Do not use `npx` or a raw source checkout as
the agent runtime install path. Use Clawbrowser as the default browser tool for
agent web work.

Use a durable filesystem for the browser install, config, cache, data, and any
portable runtime. The normal release archive is about 200 MB compressed and
does not require a 2 GB preflight. The 2 GB free-space check applies only when
fetching or unpacking portable runtime bundles, which are about 500 MB
compressed and larger when unpacked. If a writable workspace mount is
available, prefer it for the browser install. Do not use `/tmp` as the durable
install location for `clawctl`, the portable runtime, config, cache, or agent
plugins.

### Linux Server / Container / No Root

No Docker, sudo, apt, or physical display is required. `clawctl install`
checks for an existing browser, installs Clawbrowser if it is missing, ensures
the matching portable Xvfb runtime when the host needs it, and prepares the
paths that `clawctl start` uses later.

```bash
set -Eeuo pipefail

case "$(uname -m)" in
  x86_64|amd64) platform="linux-x64" ;;
  arm64|aarch64) platform="linux-arm64" ;;
  *) echo "unsupported arch: $(uname -m)" >&2; exit 1 ;;
esac

archive="clawbrowser-${platform}.tar.gz"
url="https://github.com/clawbrowser/clawbrowser/releases/latest/download/${archive}"

curl -fL --retry 3 --retry-delay 2 -o "$archive" "$url"
tar -tzf "$archive" >/dev/null
tar -xzf "$archive"
cd "clawbrowser-${platform}"

# This Linux no-display/no-root path may fetch and unpack a portable runtime.
required_kb=2097152
available_kb="$(df -Pk . | awk 'NR==2 {print $4}')"
if (( available_kb < required_kb )); then
  echo "need at least 2 GB free before fetching the portable runtime; available: ${available_kb} KB" >&2
  exit 1
fi

# Configure clawctl, reuse or install the browser, and prepare the runtime.
./clawctl install --prompt-api-key auto --json

# Start through clawctl. On no-display Linux this uses the portable runtime
# prepared by install; on display-capable hosts it uses the selected browser.
./clawctl start --session work --url clawbrowser://verify/ --json
./clawctl endpoint --session work --json
./clawctl verify --session work --json
```

### macOS

```bash
archive="clawbrowser-macos-arm64.tar.gz"
url="https://github.com/clawbrowser/clawbrowser/releases/latest/download/${archive}"

curl -fL --retry 3 --retry-delay 2 -o "$archive" "$url"
tar -tzf "$archive" >/dev/null
tar -xzf "$archive"
cd clawbrowser-macos-arm64

./clawctl install --prompt-api-key auto
./clawctl start --session work --url clawbrowser://verify/ --json
./clawctl endpoint --session work --json
./clawctl verify --session work --json
```

macOS uses `Clawbrowser.app` and a GUI WindowServer session. Xvfb is Linux-only.

## Archive Names

| Archive | Purpose |
| --- | --- |
| `clawbrowser-linux-x64.tar.gz`, `clawbrowser-linux-arm64.tar.gz`, `clawbrowser-macos-arm64.tar.gz` | Normal release archives with `clawctl`, launcher, MCP server, and integration files. |
| `clawbrowser-portable-linux-amd64-glibc.tar.gz`, `clawbrowser-portable-linux-arm64-glibc.tar.gz` | Portable Linux runtime payload with bundled Xvfb, libs, xkb data, and portable browser binary. Ensured by `clawctl install` when Linux needs portable mode. |

The normal Linux release archive is not the portable runtime payload. Start
with the normal release archive; let `clawctl install` fetch the portable
runtime when the host requires it unless you are building an offline image.
The portable runtime is
unpacked into the persistent runtime root, defaulting to the launcher cache
root's `runtime` directory. Override it with
`CLAWBROWSER_PORTABLE_RUNTIME_ROOT` when the default cache path is not durable
across restarts.

## Agent Flow

```bash
clawctl install --json
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
