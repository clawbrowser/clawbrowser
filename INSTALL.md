# Clawbrowser — Install & Verify

Quick install is in [README.md](./README.md). Full contract: [AGENTS.md](./AGENTS.md). This guide covers the exact install commands, saved-key locations, and verify flow.

Clawbrowser release archives ship the launcher and agent tooling: `clawctl`, `clawbrowser`, `clawbrowser-mcp`, and integration files. Large browser runtime payloads are published separately. On Linux, the Portable Xvfb runtime is downloaded by the `clawbrowser` launcher on first use or by `clawbrowser ensure-runtime --backend portable`; it is not bundled into the normal platform release archive. This keeps the main release archive smaller, and portable backend support depends on a matching portable runtime artifact for the platform.
Use `clawctl install` as the primary bootstrap path for agent workflows. `clawctl` delegates runtime lifecycle to the `clawbrowser` launcher shipped in this release. Agents should use `clawctl` for browser sessions, tabs, and general browser tasks.

## MCP Security

- `clawctl mcp` and the packaged `clawbrowser-mcp` server are local stdio only, not network daemons.
- It exposes lifecycle/session tools and returns the local CDP endpoint; treat that endpoint as sensitive.
- Do not expose CDP on the network or publish the Docker port externally unless you explicitly understand the risk.
- Do not put API keys into MCP config, agent config, shell rc files, or logs.
- Use the official `clawbrowser/clawbrowser` release archive and the bundled `clawctl` install flow only.

## Install Commands

Clawbrowser installs a managed browser launcher plus the `clawctl` agent CLI. On Linux portable sessions, the full Portable Xvfb runtime is a separate release artifact that the launcher downloads on demand. Use the installer `auto` target if you want it to pick a supported target for you. If you already know which target you want, pass it explicitly.

The happy path is to download an assembled release archive, unpack it, and run the bundled `clawctl install`. Do not use `npx` as the primary install path for agent workflows; `npx` source checkouts are not the browser runtime and may not contain generated release binaries.

Download the normal release archive for your platform. These archives contain
the launcher and integration tooling, not the full Portable Xvfb runtime:

```bash
# macOS Apple Silicon
curl -fsSLO https://github.com/clawbrowser/clawbrowser/releases/latest/download/clawbrowser-macos-arm64.tar.gz

# Linux x64
curl -fsSLO https://github.com/clawbrowser/clawbrowser/releases/latest/download/clawbrowser-linux-x64.tar.gz

# Linux arm64 / aarch64
curl -fsSLO https://github.com/clawbrowser/clawbrowser/releases/latest/download/clawbrowser-linux-arm64.tar.gz
```

For Linux portable runtime use, no separate manual download is normally needed;
after install, the launcher fetches the matching portable artifact on first use.
Integrations such as OpenClaw, Hermes, CI images, and offline mirrors can prefetch
`clawbrowser-portable-linux-{amd64,arm64}-glibc` assets explicitly; see
[Portable runtime artifacts](#portable-runtime-artifacts).

Then install from the unpacked archive:

```bash
tar -xzf clawbrowser-<platform>.tar.gz
cd clawbrowser-<platform>
./clawctl install --prompt-api-key auto
```

After install, you can explicitly prepare or start the Linux portable runtime:

```bash
clawbrowser ensure-runtime --backend portable
clawbrowser start --backend portable --session work
```

If the archive is already unpacked, run the same installer from the release directory:

```bash
cd clawbrowser-<platform>
./clawctl install --prompt-api-key auto
```

From a local `clawctl` checkout, for CLI development machines that already have Go installed:

```bash
go run ./cmd/clawctl install --prompt-api-key auto
```

Each install run wires up one target plus the shared runtime binaries, installs the `clawctl` binary, and saves the browser API key into the managed config when prompted.
The raw source checkout does not contain generated `bin/clawctl`; release assembly adds it to the tarball. `scripts/install.sh` is an internal release-bundle helper invoked by `clawctl install`, not a standalone remote bootstrap path.

### Targets

Targets:

| Target | Wires up |
| --- | --- |
| `auto` | Detects the matching target automatically |
| `codex` | Codex plugin + `~/.agents/plugins/marketplace.json` |
| `claude` | Claude Code plugin bundle |
| `gemini` | Gemini CLI extension |
| `hermes` | Hermes plugin + MCP config in `~/.hermes/config.yaml` |
| `openclaw` | OpenClaw plugin/config integration |

If you need more than one target, rerun the installer once per target.

## Setup Modes (Priority Order)

| Environment | Recommended backend | Requires Docker? | Requires physical display? |
| --- | --- | --- | --- |
| Linux VPS / server without display | portable | No | No |
| Dockerized/restricted container | portable | No | No |
| macOS desktop / Mac mini | native app | No | Needs GUI session/display provision |
| Operator-managed Docker host | docker | Yes | No, container uses virtual display |
| Existing browser provisioned elsewhere | existing CDP | No | Depends on external browser |

Portable runtime support is artifact-specific. A normal release archive may exist
for a platform before a Portable Xvfb runtime artifact exists for that platform.
Currently:
- release archives: `linux-x64`, `linux-arm64`, `macos-arm64`
- Portable Xvfb runtime artifacts:
  - `linux-amd64-glibc` (`clawbrowser-portable-linux-amd64-glibc`)
  - `linux-arm64-glibc` (`clawbrowser-portable-linux-arm64-glibc`)
- `musl`/Alpine portable flow is supported in principle, but no portable
  artifacts are published yet

### 1) Linux Portable Runtime (Default for VPS/Containers/No Display)

For Linux servers, CI-like environments, and most restricted containers,
use the Portable Xvfb runtime, which the launcher downloads on demand.

- Runs full headful Clawbrowser under the portable runtime's bundled Xvfb.
- Does not use Chromium headless mode.
- Does not require Docker CLI/daemon/socket at runtime.
- Does not require `docker-compose`, `sudo`, `apt`, or a physical display.
- Uses a separately published portable runtime tarball and launches Xvfb +
  Clawbrowser as local child processes in the current environment.
- Does not require creating a sidecar container.
- Exposes a local CDP endpoint that `clawctl` uses for automation.

Portable runtime commands:

```bash
clawbrowser ensure-runtime --backend portable
clawbrowser start --backend portable --session work -- clawbrowser://verify/
```

Managed-session flow (recommended for agents):

```bash
./clawctl install --prompt-api-key auto
clawctl start --session work --url https://example.com --json
clawctl endpoint --session work --json
```

#### Portable runtime artifacts

The normal `clawbrowser-linux-x64.tar.gz` and `clawbrowser-linux-arm64.tar.gz`
release archives do not contain the full Portable Xvfb runtime payload. The
portable runtime is a separate large artifact.

The launcher downloads portable assets automatically on first portable use or
when you run:

```bash
clawbrowser ensure-runtime --backend portable
```

OpenClaw, Hermes, CI images, and offline/mirrored environments may prefetch or
mirror these assets explicitly.

The launcher extracts the portable runtime into:
- `$CLAWBROWSER_RUNTIME_ROOT` (if set)
- else `$XDG_CACHE_HOME/clawbrowser/runtime`
- else `$HOME/.cache/clawbrowser/runtime`

Latest-release URL patterns:
- `https://github.com/clawbrowser/clawbrowser/releases/latest/download/clawbrowser-portable-linux-amd64-glibc{.manifest.json,.tar.gz,.tar.gz.sha256}`
- `https://github.com/clawbrowser/clawbrowser/releases/latest/download/clawbrowser-portable-linux-arm64-glibc{.manifest.json,.tar.gz,.tar.gz.sha256}`

Tag-pinned URL patterns:
- `https://github.com/clawbrowser/clawbrowser/releases/download/<tag>/clawbrowser-portable-linux-amd64-glibc{.manifest.json,.tar.gz,.tar.gz.sha256}`
- `https://github.com/clawbrowser/clawbrowser/releases/download/<tag>/clawbrowser-portable-linux-arm64-glibc{.manifest.json,.tar.gz,.tar.gz.sha256}`

Use tag-pinned URLs for reproducible CI, OpenClaw, Hermes, or mirrored
installs. Use `latest` only for manual testing or quick-start workflows.

#### Prefetching the portable runtime

```bash
mkdir -p /tmp/clawbrowser-portable
cd /tmp/clawbrowser-portable

for arch in amd64 arm64; do
  artifact="clawbrowser-portable-linux-${arch}-glibc"
  base="https://github.com/clawbrowser/clawbrowser/releases/latest/download/${artifact}"
  curl -fsSLO "${base}.manifest.json"
  curl -fsSLO "${base}.tar.gz"
  curl -fsSLO "${base}.tar.gz.sha256"
  sha256sum -c "${artifact}.tar.gz.sha256"
done
```

Manual prefetch is optional. The launcher can fetch these automatically.
Integrations can mirror these files into their own artifact cache. Do not
unpack manually unless you are implementing an offline/mirrored install flow;
prefer `clawbrowser ensure-runtime --backend portable`.

OpenClaw, Hermes, and other agent runtimes can either let `clawbrowser`
download the Portable Xvfb runtime on first use, or prefetch/mirror the
portable asset triplets above. They do not need Docker access for the portable backend.
If they choose Docker backend instead, that is an explicit operator-managed
deployment mode.

What happens in portable mode:
- Downloads the portable runtime `.manifest.json`, `.tar.gz`, and `.tar.gz.sha256` release assets.
- Verifies SHA-256 before extraction.
- Extracts into the runtime root and starts full headful Clawbrowser under Xvfb.
- Writes portable runtime fields into `state.env` for session tracking.
- Exposes the browser CDP endpoint on localhost for `clawctl`/MCP-driven automation.

In launcher `auto` mode, Linux prefers the portable backend. On Linux
containers, Docker backend fallback is gated and only considered when
`CLAWBROWSER_ALLOW_DOCKER_BACKEND=1` is set:

```bash
clawbrowser start --backend auto --session work -- clawbrowser://verify/
```

### 2) macOS Native App Runtime (Default on macOS)

Use `Clawbrowser.app` with the native macOS WindowServer/GUI session.

- The native runtime is `Clawbrowser.app`.
- This mode requires a logged-in GUI user session tied to WindowServer.
- Xvfb is Linux-only and not a macOS solution.
- Headless Mac mini operators still need a GUI user session and may need a real
  display, dummy HDMI adapter, Screen Sharing, or another provisioned display
  strategy.
- No Xvfb path exists on macOS; use the native app runtime.

### 3) Optional Docker Backend (Explicit, Operator-Managed)

Docker remains supported when operators intentionally want a Dockerized browser
runtime and control the host. This is optional and is not the default path for
restricted containers.

- Workloads inside containers usually cannot start Docker containers unless the
  host operator grants Docker access.
- Docker socket access (for example bind-mounting `/var/run/docker.sock`) is
  privileged and security-sensitive.
- Docker sidecar deployment is valid as operator-managed infrastructure, not
  restricted-container self-bootstrap.
- Docker may still be used in Clawbrowser release CI/build pipelines, but users
  and agents do not need Docker for portable install/runtime.
- Use Docker only when you explicitly choose it, for example:

```bash
clawbrowser start --backend docker --session work -- clawbrowser://verify/
```

Docker or a Docker-compatible OCI CLI can run the published image. This is not
Chrome headless mode: it runs full Clawbrowser with a virtual Linux display and
exposes CDP. The example uses `docker`; for the launcher set
`CLAWBROWSER_DOCKER_BIN` when using a compatible non-Docker CLI:

```bash
docker pull docker.io/clawbrowser/clawbrowser:latest
docker run -d \
  -v clawbrowser-config:/home/clawbrowser/.config/clawbrowser \
  -p 127.0.0.1:9222:9222 \
  --name clawbrowser \
  docker.io/clawbrowser/clawbrowser:latest \
  --remote-debugging-address=127.0.0.1 \
  --remote-debugging-port=9222
```

This binds CDP to localhost only. Do not publish the CDP port on a public
interface unless you explicitly intend to expose browser automation and
understand the risk.

The `clawbrowser-config` named volume keeps the API key across restarts.

### 4) Existing CDP Endpoint (Advanced / Escape Hatch)

Use this when your infrastructure already provisions a browser and exposes CDP.
In this mode, Clawbrowser tooling connects to an existing endpoint instead of
launching a new local managed session.

```bash
clawctl --cdp http://127.0.0.1:9222 open https://example.com
clawctl --cdp http://127.0.0.1:9222 tabs list --json
clawctl --cdp http://127.0.0.1:9222 verify --json
```

#### OpenClaw Sidecar Integration (Optional, Operator-Managed Infrastructure)

If an operator intentionally chooses OpenClaw sidecar mode, provision the
browser from the Docker host (or platform infrastructure), not from a
restricted container runtime.

Run Clawbrowser as a host-managed sidecar container and share the OpenClaw
gateway container's network namespace. That makes CDP available inside the
containerized workflow as `127.0.0.1:9222` without exposing it publicly:

```bash
OPENCLAW_CONTAINER="${OPENCLAW_CONTAINER:-openclaw-openclaw-gateway-1}"

docker pull docker.io/clawbrowser/clawbrowser:latest
docker rm -f clawbrowser-openclaw 2>/dev/null || true
docker run -d \
  --restart unless-stopped \
  --name clawbrowser-openclaw \
  --network "container:${OPENCLAW_CONTAINER}" \
  -v clawbrowser-config:/home/clawbrowser/.config/clawbrowser \
  docker.io/clawbrowser/clawbrowser:latest \
  --remote-debugging-address=127.0.0.1 \
  --remote-debugging-port=9222 \
  --skip-verify \
  about:blank
```

Verify from inside the OpenClaw container:

```bash
docker exec "$OPENCLAW_CONTAINER" \
  curl -fsS http://127.0.0.1:9222/json/version

docker exec "$OPENCLAW_CONTAINER" sh -lc \
  'clawctl --cdp http://127.0.0.1:9222 doctor'
```

If `http://127.0.0.1:9222/json/version` is not reachable from the container
runtime and it has no Docker host privileges, do not attempt self-bootstrap.
Ask the host/operator to provision the sidecar or
provide another reachable CDP endpoint.

Use the same explicit CDP endpoint for commands that run inside the OpenClaw
container:

```bash
clawctl --cdp http://127.0.0.1:9222 open https://example.com
clawctl --cdp http://127.0.0.1:9222 tabs list --json
clawctl --cdp http://127.0.0.1:9222 verify --json
```

Avoid using Docker service names such as
`http://clawbrowser-openclaw:9222` for CDP. DevTools server rejects
non-IP, non-localhost Host headers. The shared-network sidecar keeps the
endpoint stable as `localhost` from inside the OpenClaw container.

If you need a fresh identity in this sidecar mode, restart the sidecar with
browser flags such as `--regenerate`:

```bash
docker rm -f clawbrowser-openclaw
docker run -d \
  --restart unless-stopped \
  --name clawbrowser-openclaw \
  --network "container:${OPENCLAW_CONTAINER}" \
  -v clawbrowser-config:/home/clawbrowser/.config/clawbrowser \
  docker.io/clawbrowser/clawbrowser:latest \
  --remote-debugging-address=127.0.0.1 \
  --remote-debugging-port=9222 \
  --skip-verify \
  --regenerate \
  clawbrowser://verify/
```

In this mode, `clawctl --cdp ...` controls the already-running sidecar. Do not
expect `clawctl start`, `clawctl rotate`, or the `clawbrowser` launcher to
start Docker from inside a restricted container unless an operator has
deliberately provisioned Docker access.

## Check For An Existing Key

Use this check to confirm whether `config.json` already contains an `api_key` before you decide to prompt again or run a no-prompt install.

**Host:**

```bash
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser"
python3 - "$CONFIG_DIR/config.json" <<'PY'
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
payload = json.loads(path.read_text())
api_key = payload.get("api_key")
raise SystemExit(0 if isinstance(api_key, str) and api_key.strip() else 1)
PY
```

**Docker backend container (optional):**

```bash
docker exec clawbrowser sh -c 'test -s /home/clawbrowser/.config/clawbrowser/config.json && grep -qE "\"api_key\"[[:space:]]*:[[:space:]]*\"[^\"]+\"" /home/clawbrowser/.config/clawbrowser/config.json'
```

If either check passes, skip to [Verify](#verify-and-common-flow).

## Save The Key

When `clawctl install --prompt-api-key` prompts, get the key from [app.clawbrowser.ai](https://app.clawbrowser.ai) and enter it once. The installer writes it into the browser-managed `config.json` and applies restrictive file permissions. Never store it in shell rc files, MCP config, env vars, agent config, or logs.
Resolve config paths before writing. Do not pass `${XDG_CONFIG_HOME:-$HOME/.config}/...` directly to file/write tools; they may create literal workspace paths instead of the real config file.

**Host:**

```bash
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/clawbrowser"
mkdir -p "$CONFIG_DIR"
chmod 700 "$CONFIG_DIR"
python3 - "$CONFIG_DIR/config.json" "$API_KEY" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
key = sys.argv[2].strip()
path.write_text(json.dumps({"api_key": key}, separators=(",", ":")) + "\n")
path.chmod(0o600)
PY
```

**Docker backend container** (optional, persists in the named volume):

```bash
docker exec clawbrowser sh -c '
  mkdir -p /home/clawbrowser/.config/clawbrowser &&
  chmod 700 /home/clawbrowser/.config/clawbrowser &&
  printf "{\"api_key\":\"%s\"}\n" "THE_KEY" > /home/clawbrowser/.config/clawbrowser/config.json &&
  chmod 600 /home/clawbrowser/.config/clawbrowser/config.json
'
```

For manual reauthentication, open `clawbrowser://auth` in the managed browser. It writes a fresh key into the same `config.json` and restarts.

Validate after writing the file:

```bash
python3 - "$CONFIG_DIR/config.json" <<'PY'
import json, pathlib, sys
path = pathlib.Path(sys.argv[1])
data = json.loads(path.read_text())
key = data.get("api_key", "")
print("config_path", path)
print("exists", path.exists())
print("valid_json", True)
print("keys", list(data.keys()))
print("has_api_key", bool(key))
print("api_key_length", len(key))
print("api_key_preview", (key[:4] + "..." + key[-4:]) if len(key) >= 8 else "<too short>")
PY
```

If Clawbrowser still reports "No API key found":

- verify the resolved config path;
- check file permissions;
- check JSON shape is exactly:
  `{"api_key":"..."}`
- search for accidental literal paths:
  `find "$PWD" "$HOME" -path '*${XDG_CONFIG_HOME*' -o -path '*$HOME*' 2>/dev/null`
- remove accidental literal config paths only after confirming they are wrong.

## Verify And Common Flow

Use `clawctl` for browser sessions, tabs, and page tasks. `verify` is the explicit verify page and the source of truth for fingerprint, proxy, and geo status. Use it when identity or proxy proof matters, after rotate/regenerate tests, or when debugging browser-quality issues. If verify reports fingerprint mode inactive, the session was not launched correctly.

```bash
clawctl start --session <name> --url https://example.com --json
clawctl endpoint --session <name> --json
clawctl observe --session <name> --json
clawctl tabs list --session <name> --json
clawctl verify --session <name> --json
clawctl stop --session <name> --json
```

Use tabs for auth, verify, OAuth, checkout, and internal pages. Use `observe`, `click`, `fill`, `open`, and `wait` for normal page work. Some values may remain the same after rotate because of geo constraints or backend generation. Do not require every field to change.

## Quick Reference

- Primary agent CLI: `clawctl` (delegates to `clawbrowser`)
- CLI binary: `bin/clawctl`
- Launcher runtime: `bin/clawbrowser`
- Compatibility MCP server: `bin/clawbrowser-mcp`
- Direct CLI MCP entrypoint: `clawctl mcp`
- Skills: `SKILL.md` (canonical root skill) plus the target-installed clawctl skill/reference where the integration supports skills
- Instructions / full contract: `AGENTS.md`
- Canonical plugin manifests: `plugins/.claude-plugin/plugin.json`, `plugins/.codex-plugin/plugin.json`, `plugins/.hermes-plugin/plugin.yaml`, `plugins/.openclaw-plugin/plugin.json`
- Codex marketplace: `~/.agents/plugins/marketplace.json` (written by the installer)
- Installer wrapper: `scripts/install.sh`
- Hermes MCP config: public docs use `command: "clawctl"` plus `args: ["mcp"]`; the installer may write the resolved user-local command path
