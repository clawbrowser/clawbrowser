# Clawbrowser - Install & Run

Use the standalone `clawctl` release archive for the host OS/arch. The browser
archive is not the bootstrapper. `clawctl install` owns browser and portable
runtime download/install. Installing only the `clawctl` agent skill or plugin is
not enough; the skill teaches commands, but the standalone `clawctl` binary and
managed Clawbrowser runtime still need to be installed.

## Agent Fast Path

Agent integration templates live in `clawctl` and are installed through
`clawctl install`. This release repository must not be used as a source for
manual `.mcp.json`, Claude, Codex, Gemini, Hermes, or OpenClaw plugin setup.

This is the path agents should follow on Linux servers, restricted containers,
CI-like hosts, and macOS GUI hosts. Do not start from a browser archive, raw
source checkout, Docker, a manual runtime download, or `--runtime-version`.

Copy the script below instead of rewriting it. It deliberately downloads,
extracts, installs, and runs `clawctl` from one executable workdir. Never set
`CLAWBROWSER_WORKDIR` to `/tmp` or extract `clawctl` under `/tmp`; many agent
containers mount `/tmp` with `noexec`, which causes `Permission denied` even
after `chmod +x`.

Steps with the exact commands:

1. Find a durable writable directory outside `/tmp`, then set the Clawbrowser
   config/cache/data paths under it. Do not set generic agent plugin overrides;
   `clawctl install` must see the active agent environment so it can write
   Hermes plugins, Gemini extensions, Codex plugins, and other integrations to
   the paths those agents actually scan. Preserve the current `HOME` when it is
   a real user/agent home; only replace it for empty, `/root`, or `/tmp` homes.
2. Download and extract the standalone `clawctl` archive:
   `curl -fL -o "$archive" "$url" && tar -xzf "$archive"` from inside that
   workdir, not `/tmp`.
3. Install Clawbrowser:
   `"$clawctl_bootstrap" install --json`.
4. Ask once for the Clawbrowser API key from `https://app.clawbrowser.ai`.
5. Save the API key:
   `printf '%s\n' "$CLAWBROWSER_API_KEY" | "$clawctl" config set api-key --stdin`.
6. Verify with these commands:

   ```bash
   "$clawctl" start --profile work --url clawbrowser://verify/ --json
   "$clawctl" endpoint --profile work --json
   "$clawctl" verify --profile work --json
   ```

The full copy/paste script is below.

Use one shell variable for the key only long enough to save it. Do not put API
keys in shell rc files, MCP config, agent config, env files, logs, or command
arguments.

```bash
set -Eeuo pipefail

original_home="${HOME:-}"
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

mkdir -p "$CLAWBROWSER_WORKDIR/home" "$CLAWBROWSER_WORKDIR/config" "$CLAWBROWSER_WORKDIR/cache" "$CLAWBROWSER_WORKDIR/data" "$CLAWBROWSER_WORKDIR/bin"
case "${original_home:-}" in
  ""|/root|/root/*|/tmp|/tmp/*) export HOME="$CLAWBROWSER_WORKDIR/home" ;;
  *) export HOME="$original_home" ;;
esac
export XDG_CONFIG_HOME="$CLAWBROWSER_WORKDIR/config"
export XDG_CACHE_HOME="$CLAWBROWSER_WORKDIR/cache"
export XDG_DATA_HOME="$CLAWBROWSER_WORKDIR/data"
export CLAWBROWSER_BIN_DIR="$CLAWBROWSER_WORKDIR/bin"
export CLAWBROWSER_PORTABLE_RUNTIME_ROOT="$XDG_CACHE_HOME/clawbrowser/runtime"
mkdir -p \
  "$CLAWBROWSER_BIN_DIR" \
  "$CLAWBROWSER_PORTABLE_RUNTIME_ROOT"
cd "$CLAWBROWSER_WORKDIR"

case "$(uname -s):$(uname -m)" in
  Linux:x86_64|Linux:amd64) platform="linux-amd64" ;;
  Linux:arm64|Linux:aarch64) platform="linux-arm64" ;;
  Darwin:arm64) platform="macos-arm64" ;;
  *) echo "unsupported host: $(uname -s) $(uname -m)" >&2; exit 1 ;;
esac

archive="clawctl-${platform}.tar.gz"
url="https://github.com/clawbrowser/clawbrowser/releases/latest/download/${archive}"

curl -fL --retry 3 --retry-delay 2 -o "$archive" "$url"
tar -tzf "$archive" >/dev/null
rm -rf "clawctl-${platform}"
tar -xzf "$archive"
clawctl_bootstrap="$CLAWBROWSER_WORKDIR/clawctl-${platform}/clawctl"
[ -x "$clawctl_bootstrap" ] || { echo "missing clawctl in extracted archive" >&2; exit 1; }

"$clawctl_bootstrap" install --json

clawctl="$CLAWBROWSER_BIN_DIR/clawctl"
[ -x "$clawctl" ] || clawctl="$clawctl_bootstrap"

if [ -z "${CLAWBROWSER_API_KEY:-}" ]; then
  printf "Clawbrowser API key from https://app.clawbrowser.ai: " >&2
  stty -echo 2>/dev/null || true
  IFS= read -r CLAWBROWSER_API_KEY || true
  stty echo 2>/dev/null || true
  printf "\n" >&2
fi
[ -n "$CLAWBROWSER_API_KEY" ] || { echo "Clawbrowser API key is required" >&2; exit 1; }
printf '%s\n' "$CLAWBROWSER_API_KEY" | "$clawctl" config set api-key --stdin
unset CLAWBROWSER_API_KEY

"$clawctl" start --profile work --url clawbrowser://verify/ --json
"$clawctl" endpoint --profile work --json
"$clawctl" verify --profile work --json
```

Linux portable installs need enough free space for the runtime bundle, which is
about 500 MB compressed and larger after unpacking. If install reports a space
error or `Permission denied` while running `clawctl`, rerun the same script with
`CLAWBROWSER_WORKDIR` set to a larger durable executable path such as
`/workspace/.clawbrowser`, `/work/.clawbrowser`, or `$PWD/.clawbrowser`.

## macOS Fast Path

Use the standalone macOS `clawctl` archive from a logged-in GUI session:

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

macOS uses `Clawbrowser.app` and WindowServer. Xvfb is Linux-only.

## Which Archive Is Which

| Thing | Use it for | Notes |
| --- | --- | --- |
| `clawctl-linux-amd64.tar.gz`, `clawctl-linux-arm64.tar.gz`, `clawctl-macos-arm64.tar.gz` | Bootstrap install | Standalone `clawctl` archives. Start here. |
| `clawbrowser-linux-amd64.tar.gz`, `clawbrowser-linux-arm64.tar.gz`, `clawbrowser-macos-arm64.tar.gz` | Browser payload | `clawctl install` downloads one when no usable browser exists. Agent integrations are materialized by `clawctl install`; do not install plugin specs from this repository manually. |
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

./clawctl install --json
./clawctl config set api-key
./clawctl start --profile work --url clawbrowser://verify/ --json
./clawctl endpoint --profile work --json
./clawctl verify --profile work --json
```

`CLAWBROWSER_PORTABLE_LOCAL_DIR` can point either at the extracted platform
directory (`linux-amd64-glibc`) or its parent directory.

## Restricted Agent Containers

Use the same [Agent Fast Path](#agent-fast-path). Do not build a custom matrix
of path flags. If the automatic workdir probe fails, set one durable writable
executable directory and rerun the same script:

```bash
export CLAWBROWSER_WORKDIR="/workspace/.clawbrowser"
```

Use a real workspace mount, `/workspace`, `/work`, `$PWD/.clawbrowser`, or
another persistent operator-provided directory. Do not use `/tmp` for the
archive, extraction, install, config, cache, data, portable runtime, or plugin
location. Do not try Docker as a fallback inside restricted agent containers;
use portable mode from `clawctl install` or an operator-provided CDP endpoint.

## Canonical Agent Flow

Use `clawctl` for profile lifecycle and use CDP for page automation.

```bash
clawctl install --json
clawctl config set api-key
clawctl start --profile work --url clawbrowser://verify/ --json
clawctl endpoint --profile work --json
clawctl verify --profile work --json
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
clawctl start --profile work --url clawbrowser://verify/ --json
clawctl verify --profile work --json
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
clawctl start --backend docker --profile work --url clawbrowser://verify/ --json
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
| `Permission denied` when running `clawctl`, even after `chmod +x` | The archive was extracted under `/tmp` or another `noexec` filesystem. The tarball is not the problem. | Stop trying `chmod`. Set `CLAWBROWSER_WORKDIR` to a durable executable directory such as `/workspace/.clawbrowser`, `/work/.clawbrowser`, or `$PWD/.clawbrowser`, then rerun the [Agent Fast Path](#agent-fast-path). |
| Read-only `/root` or installer tries to write under `/root` | Restricted agent container set `HOME=/root`, but `/root` is not usable for durable state. | Set `CLAWBROWSER_WORKDIR` to one durable writable directory, such as `/workspace/.clawbrowser`, then rerun the [Agent Fast Path](#agent-fast-path). |
| `Required command not found: docker` | Docker backend was selected, or an old/source launcher path is being used. | Rerun `clawctl install` and `clawctl start` with the release `clawctl`; use `--backend portable` only when explicitly forcing portable mode. |
| Docker socket or permission error | Restricted container cannot self-provision Docker. | Use portable mode, or ask the operator for `clawctl --cdp http://127.0.0.1:9222 ...`. |
| `portable runtime not found; set CLAWBROWSER_PORTABLE_LOCAL_DIR or run clawctl install` | The self-contained launcher could not find an installed portable runtime. | Set `CLAWBROWSER_PORTABLE_LOCAL_DIR` to a pre-extracted runtime, or rerun `clawctl install` with writable cache/data paths. |
| Portable artifact missing | Current release lacks the matching portable runtime asset. | Check the release assets for `clawbrowser-portable-linux-amd64-glibc.tar.gz` or `clawbrowser-portable-linux-arm64-glibc.tar.gz`, or pin to a release that has it. |
| Portable checksum missing | Current release lacks the matching `.sha256` asset for the portable runtime. | Publish the checksum asset or pin to a complete release; the launcher refuses unchecked portable runtime installs. |
| Alpine/musl error | Portable artifacts are glibc builds. | Use a glibc image, operator-managed Docker, or an external CDP endpoint. |
| Endpoint refused or stale | Profile restarted or endpoint changed. | Run `clawctl endpoint --profile <name> --json`; if still down on Linux no-monitor hosts, run `clawctl start --profile <name> --url clawbrowser://verify/ --json`. |

## Quick Reference

- Start or reattach: `clawctl start --profile work --url https://example.com --json`
- Current endpoint: `clawctl endpoint --profile work --json`
- Fresh identity: `clawctl rotate --profile work --url clawbrowser://verify/ --json`
- Live runtime profiles: `clawctl sessions list --json`
- Cached profiles: `clawctl list --profile work --json`
- Stop: `clawctl stop --profile work --json`
