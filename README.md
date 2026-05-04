# Clawbrowser

Clawbrowser is a managed browser runtime for AI agents.

Full agent contract: [AGENTS.md](./AGENTS.md)

Install details and troubleshooting: [INSTALL.md](./INSTALL.md)

## Install

Use the standalone `clawctl` release archive for the host OS/arch. The browser
archive is not the bootstrapper. `clawctl install` owns browser and portable
runtime download/install.

Use a durable filesystem for the browser install, config, cache, data, and any
portable runtime. The standalone `clawctl` archive is small and does not
require a 2 GB preflight. The 2 GB free-space check applies only when fetching
or unpacking portable runtime bundles, which are about 500 MB compressed and
larger when unpacked. If a writable workspace mount is available, prefer it for
the browser install. Do not use `/tmp` as the durable install location for
`clawctl`, the portable runtime, config, cache, or agent plugins.

### Linux Server / Container / No Root

No Docker, sudo, apt, or physical display is required. `clawctl install`
checks for an existing browser, installs Clawbrowser if it is missing, ensures
the matching portable Xvfb runtime when the host needs it, and prepares the
paths that `clawctl start` uses later.

```bash
set -Eeuo pipefail

selected_workdir=""
for candidate in \
  "${CLAWBROWSER_WORKDIR:-}" \
  "$PWD/.clawbrowser" \
  "${WORKSPACE:+$WORKSPACE/.clawbrowser}" \
  "${GITHUB_WORKSPACE:+$GITHUB_WORKSPACE/.clawbrowser}" \
  "/workspace/.clawbrowser" \
  "/work/.clawbrowser" \
  "${HOME:+$HOME/.clawbrowser}"; do
  [ -n "$candidate" ] || continue
  case "$candidate" in /tmp|/tmp/*) continue ;; esac
  mkdir -p "$candidate" 2>/dev/null || continue
  probe="$candidate/.exec-probe"
  printf '#!/bin/sh\nexit 0\n' > "$probe" 2>/dev/null || continue
  chmod +x "$probe" 2>/dev/null || continue
  "$probe" >/dev/null 2>&1 || { rm -f "$probe"; continue; }
  rm -f "$probe"
  selected_workdir="$candidate"
  break
done
[ -n "$selected_workdir" ] || { echo "no writable executable Clawbrowser workdir found" >&2; exit 1; }
export CLAWBROWSER_WORKDIR="$selected_workdir"

mkdir -p "$CLAWBROWSER_WORKDIR/config" "$CLAWBROWSER_WORKDIR/cache" "$CLAWBROWSER_WORKDIR/data"
export XDG_CONFIG_HOME="$CLAWBROWSER_WORKDIR/config"
export XDG_CACHE_HOME="$CLAWBROWSER_WORKDIR/cache"
export XDG_DATA_HOME="$CLAWBROWSER_WORKDIR/data"
runtime_root="${CLAWBROWSER_PORTABLE_RUNTIME_ROOT:-$XDG_CACHE_HOME/clawbrowser/runtime}"
mkdir -p "$runtime_root"
cd "$CLAWBROWSER_WORKDIR"

# Linux no-display/no-root hosts may fetch and unpack a portable runtime during
# clawctl install. Check that durable runtime location before install starts.
required_kb=2097152
available_kb="$(df -Pk "$runtime_root" | awk 'NR==2 {print $4}')"
if (( available_kb < required_kb )); then
  echo "need at least 2 GB free before fetching the portable runtime; available: ${available_kb} KB" >&2
  exit 1
fi
export CLAWBROWSER_PORTABLE_RUNTIME_ROOT="$runtime_root"

case "$(uname -m)" in
  x86_64|amd64) platform="linux-amd64" ;;
  arm64|aarch64) platform="linux-arm64" ;;
  *) echo "unsupported arch: $(uname -m)" >&2; exit 1 ;;
esac

archive="clawctl-${platform}.tar.gz"
url="https://github.com/clawbrowser/clawbrowser/releases/latest/download/${archive}"

curl -fL --retry 3 --retry-delay 2 -o "$archive" "$url"
tar -tzf "$archive" >/dev/null
rm -rf "clawctl-${platform}"
tar -xzf "$archive"
cd "clawctl-${platform}"

./clawctl config set api-key
./clawctl install --json

# Start through clawctl. On no-display Linux this uses the portable runtime
# prepared by install; on display-capable hosts it uses the selected browser.
./clawctl start --session work --url clawbrowser://verify/ --json
./clawctl endpoint --session work --json
./clawctl verify --session work --json
```

### macOS

```bash
archive="clawctl-macos-arm64.tar.gz"
url="https://github.com/clawbrowser/clawbrowser/releases/latest/download/${archive}"

curl -fL --retry 3 --retry-delay 2 -o "$archive" "$url"
tar -tzf "$archive" >/dev/null
tar -xzf "$archive"
cd clawctl-macos-arm64

./clawctl config set api-key
./clawctl install --json
./clawctl start --session work --url clawbrowser://verify/ --json
./clawctl endpoint --session work --json
./clawctl verify --session work --json
```

macOS uses `Clawbrowser.app` and a GUI WindowServer session. Xvfb is Linux-only.

## Archive Names

| Archive | Purpose |
| --- | --- |
| `clawctl-linux-amd64.tar.gz`, `clawctl-linux-arm64.tar.gz`, `clawctl-macos-arm64.tar.gz` | Standalone bootstrapper archives. Start here. |
| `clawbrowser-linux-amd64.tar.gz`, `clawbrowser-linux-arm64.tar.gz`, `clawbrowser-macos-arm64.tar.gz` | Browser payload archives. `clawctl install` downloads one when no usable browser exists. |
| `clawbrowser-portable-linux-amd64-glibc.tar.gz`, `clawbrowser-portable-linux-arm64-glibc.tar.gz` | Portable Linux runtime payload with bundled Xvfb, libs, xkb data, and portable browser binary. `clawctl install` downloads one when Linux needs portable mode. |

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
