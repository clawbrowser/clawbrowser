# Clawbrowser - Install & Run

Use the standalone `clawctl` release archive for the host OS/arch. The browser
archive is not the bootstrapper. `clawctl install` owns browser and portable
runtime download/install.

## Fast Path: Linux Server / Container / No Root

This is the default path for Linux VPS, CI-like environments, restricted
containers, and machines without a display. It does not require Docker, sudo,
apt, or a physical display. `clawctl install` checks for an existing
Clawbrowser, installs the matching release artifact when the browser is
missing, ensures the matching portable Xvfb runtime when the host needs it,
and prepares the paths that `clawctl start` uses later.

Use a durable filesystem for the browser install, config, cache, data, and any
portable runtime. The standalone `clawctl` archive is small and does not
require a 2 GB preflight. The 2 GB free-space check applies only when fetching
or unpacking portable runtime bundles, which are about 500 MB compressed and
larger when unpacked. If a writable workspace mount is available, prefer it for
the browser install. Do not treat `/tmp` as durable storage for `clawctl`, the
portable runtime, config, cache, or agent plugins.

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

## macOS Fast Path

Use the standalone macOS `clawctl` archive from a logged-in GUI session:

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

macOS uses `Clawbrowser.app` and WindowServer. Xvfb is Linux-only.

## Which Archive Is Which

| Thing | Use it for | Notes |
| --- | --- | --- |
| `clawctl-linux-amd64.tar.gz`, `clawctl-linux-arm64.tar.gz`, `clawctl-macos-arm64.tar.gz` | Bootstrap install | Standalone `clawctl` archives. Start here. |
| `clawbrowser-linux-amd64.tar.gz`, `clawbrowser-linux-arm64.tar.gz`, `clawbrowser-macos-arm64.tar.gz` | Browser payload | `clawctl install` downloads one when no usable browser exists. |
| `clawbrowser-portable-linux-amd64-glibc.tar.gz`, `clawbrowser-portable-linux-arm64-glibc.tar.gz` | Linux portable runtime | Contains the bundled Xvfb, libraries, xkb data, and portable browser binary. `clawctl install` ensures it when Linux needs portable mode unless you prefetch it. |
| Raw source checkout | Development only | Does not represent the installed agent runtime. Use `go run ./cmd/clawctl ...` only while developing the CLI. |
| Docker image | Operator-managed runtime | Optional. Use only when infrastructure intentionally provides Docker or a CDP sidecar. |

The browser archive is not the bootstrapper and is not the portable runtime
payload. Start with the standalone `clawctl` archive so `clawctl install` can
ensure the browser and portable Xvfb runtime, or set
`CLAWBROWSER_PORTABLE_LOCAL_DIR` to a pre-extracted portable runtime.
Downloaded portable runtimes are unpacked into the persistent runtime root,
defaulting to the launcher cache root's `runtime` directory. Set
`CLAWBROWSER_PORTABLE_RUNTIME_ROOT` to place it on a durable mounted path.

## Already Have The Portable Runtime

Use this when an agent or CI image already downloaded and extracted the portable
runtime tarball.

```bash
export CLAWBROWSER_PORTABLE_LOCAL_DIR="/absolute/path/to/linux-amd64-glibc"
# or: /absolute/path/to/linux-arm64-glibc

./clawctl config set api-key
./clawctl install --json
./clawctl start --session work --url clawbrowser://verify/ --json
./clawctl endpoint --session work --json
./clawctl verify --session work --json
```

`CLAWBROWSER_PORTABLE_LOCAL_DIR` can point either at the extracted platform
directory (`linux-amd64-glibc`) or its parent directory.

## Restricted Agent Containers: Writable Paths

Use this path when the agent runtime has no root access, no display, and a
read-only `$HOME`, including containers where `HOME=/root` but `/root` cannot be
written. Do not write to `/root` just because `HOME=/root`. If a writable
workspace mount is available, prefer it over `/tmp`. Do not execute `clawctl`
from `/tmp`; restricted containers may mount `/tmp` with `noexec`.

Pick one writable base directory on a persistent volume, preferably the
workspace mount when one is available, confirm it has enough free space for
the 500 MB compressed portable runtime tarball and unpacked runtime, and make
all Clawbrowser paths explicit:

```bash
set -Eeuo pipefail

export CLAWBROWSER_WRITABLE_ROOT="${CLAWBROWSER_WRITABLE_ROOT:?set this to a writable persistent directory outside /tmp}"
required_kb=2097152
available_kb="$(df -Pk "$CLAWBROWSER_WRITABLE_ROOT" | awk 'NR==2 {print $4}')"
if (( available_kb < required_kb )); then
  echo "need at least 2 GB free before fetching the portable runtime; available: ${available_kb} KB" >&2
  exit 1
fi

mkdir -p \
  "$CLAWBROWSER_WRITABLE_ROOT/home" \
  "$CLAWBROWSER_WRITABLE_ROOT/config" \
  "$CLAWBROWSER_WRITABLE_ROOT/cache" \
  "$CLAWBROWSER_WRITABLE_ROOT/data"

export HOME="$CLAWBROWSER_WRITABLE_ROOT/home"
export XDG_CONFIG_HOME="$CLAWBROWSER_WRITABLE_ROOT/config"
export XDG_CACHE_HOME="$CLAWBROWSER_WRITABLE_ROOT/cache"
export XDG_DATA_HOME="$CLAWBROWSER_WRITABLE_ROOT/data"

./clawctl config set api-key
./clawctl install \
  --install-root "$XDG_DATA_HOME/clawbrowser/runtime" \
  --bin-dir "$XDG_DATA_HOME/clawbrowser/bin" \
  --config-dir "$XDG_CONFIG_HOME/clawbrowser" \
  --cache-dir "$XDG_CACHE_HOME/clawbrowser" \
  --data-dir "$XDG_DATA_HOME/clawbrowser" \
  --agent-config "$XDG_CONFIG_HOME/clawbrowser/agent-marketplace.json" \
  --agent-plugins-dir "$XDG_DATA_HOME/clawbrowser/agent-plugins" \
  --json

export PATH="$XDG_DATA_HOME/clawbrowser/bin:$PATH"

# No-monitor / no-display proof path through clawctl.
clawctl start --session work --url clawbrowser://verify/ --json
clawctl endpoint --session work --json
clawctl verify --session work --json
```

The generic installer path overrides are:

| CLI flag | Environment variable |
| --- | --- |
| `--install-root` | `CLAWBROWSER_INSTALL_ROOT` |
| `--bin-dir` | `CLAWBROWSER_BIN_DIR` |
| `--config-dir` | `CLAWBROWSER_CONFIG_DIR` |
| `--cache-dir` | `CLAWBROWSER_CACHE_DIR` |
| `--data-dir` | `CLAWBROWSER_DATA_DIR` |
| `--agent-config` | `CLAWBROWSER_AGENT_CONFIG` |
| `--agent-plugins-dir` | `CLAWBROWSER_AGENT_PLUGINS_DIR` |

Path resolution order is: CLI flags, then `CLAWBROWSER_*` environment
variables, then XDG directories, then a `$HOME` fallback only when `$HOME`
exists and is writable. The installer fails fast if any resolved directory or
config parent cannot be created or written. If the only writable directory is
`/tmp`, stop and ask for a persistent mount instead of installing there.

Docker is not a fallback inside restricted agent containers. Use
portable mode prepared by `clawctl install`, or connect to an
operator-provided CDP endpoint
with `clawctl --cdp http://127.0.0.1:9222 ...`.

## Canonical Agent Flow

Use `clawctl` for session lifecycle and use CDP for page automation.

```bash
clawctl install --json
clawctl start --session work --url https://example.com --json
clawctl endpoint --session work --json
```

Attach your CDP client to the returned endpoint. Re-fetch the endpoint after
start, reattach, restart, rotate, or any connection failure. Do not persist
`ws://127.0.0.1/...` or `http://127.0.0.1:...` endpoints in config files.

If a browser is already running and exposes CDP, connect explicitly:

```bash
clawctl --cdp http://127.0.0.1:9222 tabs list --json
clawctl --cdp http://127.0.0.1:9222 verify --json
clawctl --cdp http://127.0.0.1:9222 open https://example.com
```

## Verify And Auth

`clawbrowser://verify/` is the proof page for fingerprint, proxy, and geo
state. Use it after launch, after rotate/regenerate, or when identity/proxy
correctness matters.

```bash
clawctl start --session work --url clawbrowser://verify/ --json
clawctl verify --session work --json
```

If no saved key exists, run `clawctl config set api-key` once with the real API
key from `https://app.clawbrowser.ai`:

```bash
clawctl config set api-key
```

The key is written to the browser-managed `config.json`. Do not put API keys in
MCP config, agent config, shell rc files, env files, logs, or positional shell
arguments.

On regular hosts, the default config path resolves like this:

```bash
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser"
```

In restricted containers, set `XDG_CONFIG_HOME` to a writable directory as shown
above instead of relying on `$HOME`.

Resolve that path before writing. Do not pass literal strings such as
`${XDG_CONFIG_HOME:-$HOME/.config}/...`, `$HOME/...`, or `~/...` to file-write
tools.

## Setup Modes

| Environment | Use | Docker? | Display? |
| --- | --- | --- | --- |
| Linux VPS/server/no display | portable | No | No |
| Restricted container/no root | portable | No | No |
| macOS desktop/Mac mini | native app | No | GUI session required |
| Operator-managed Docker host | docker | Yes | No physical display |
| Existing browser/CDP sidecar | `clawctl --cdp ...` | No from agent | External owner |

Docker is not a fallback agents should self-provision inside restricted
containers. If Docker socket access is unavailable or denied, use portable mode
or ask the operator for a CDP endpoint.

## Optional Docker Backend

Use this only when the operator intentionally provides Docker infrastructure.

```bash
clawctl start --backend docker --session work --url clawbrowser://verify/ --json
```

For a host-managed sidecar, keep CDP bound to localhost:

```bash
docker run -d \
  --name clawbrowser \
  -v clawbrowser-config:/home/clawbrowser/.config/clawbrowser \
  -p 127.0.0.1:9222:9222 \
  docker.io/clawbrowser/clawbrowser:latest \
  --remote-debugging-address=127.0.0.1 \
  --remote-debugging-port=9222

clawctl --cdp http://127.0.0.1:9222 verify --json
```

Do not publish CDP on a public interface unless you explicitly intend to expose
browser automation.

## Troubleshooting

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| `clawctl: command not found` | Standalone `clawctl` archive was not installed or the installed binary directory is not on `PATH`. | Run `./clawctl ...` from the unpacked standalone archive, or add the install bin directory to `PATH`. |
| `./clawctl` missing | You are in a raw source checkout or incomplete bundle. | Download the standalone `clawctl` archive and rerun `./clawctl install`. |
| Read-only `/root` or installer tries to write under `/root` | Restricted agent container set `HOME=/root`, but `/root` is not writable. | Set `HOME`, `XDG_CONFIG_HOME`, `XDG_CACHE_HOME`, and `XDG_DATA_HOME` to writable paths, confirm the target filesystem has enough free space, and pass the generic path overrides from [Restricted Agent Containers: Writable Paths](#restricted-agent-containers-writable-paths). |
| `Required command not found: docker` | Docker backend was selected, or an old/source launcher path is being used. | Rerun `clawctl install` and `clawctl start` with the release `clawctl`; use `--backend portable` only when explicitly forcing portable mode. |
| Docker socket or permission error | Restricted container cannot self-provision Docker. | Use portable mode, or ask the operator for `clawctl --cdp http://127.0.0.1:9222 ...`. |
| `portable runtime not found; set CLAWBROWSER_PORTABLE_LOCAL_DIR or run clawctl install` | The self-contained launcher could not find an installed portable runtime. | Set `CLAWBROWSER_PORTABLE_LOCAL_DIR` to a pre-extracted runtime, or rerun `clawctl install` with writable cache/data paths. |
| Portable artifact missing | Current release lacks the matching portable runtime asset. | Check the release assets for `clawbrowser-portable-linux-amd64-glibc.tar.gz` or `clawbrowser-portable-linux-arm64-glibc.tar.gz`, or pin to a release that has it. |
| Portable checksum missing | Current release lacks the matching `.sha256` asset for the portable runtime. | Publish the checksum asset or pin to a complete release; the launcher refuses unchecked portable runtime installs. |
| Alpine/musl error | Portable artifacts are glibc builds. | Use a glibc image, operator-managed Docker, or an external CDP endpoint. |
| Endpoint refused or stale | Session restarted or endpoint changed. | Run `clawctl endpoint --session <name> --json`; if still down on Linux no-monitor hosts, run `clawctl start --session <name> --url clawbrowser://verify/ --json`. |

## Quick Reference

- Start or reattach: `clawctl start --session work --url https://example.com --json`
- Current endpoint: `clawctl endpoint --session work --json`
- Fresh identity: `clawctl rotate --session work --url clawbrowser://verify/ --json`
- Live sessions: `clawctl sessions list --json`
- Cached profiles: `clawctl list --session work --json`
- Stop: `clawctl stop --session work --json`
