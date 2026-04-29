# Clawbrowser

Clawbrowser is a managed browser runtime for AI agents. Lifecycle and identity come from the CLI/MCP layer; CDP is the page automation layer. Managed sessions are expected to run in fingerprint/proxy mode, and the browser never touches the human's personal browser profile.

Full contract: [AGENTS.md](./AGENTS.md)

## How It Works

- `clawctl` creates or reattaches managed sessions and exposes a local CDP endpoint.
- `clawctl mcp` provides the same lifecycle and identity controls through MCP.
- Release archives install `clawctl`, `clawbrowser`, and `clawbrowser-mcp`.
- CDP is the page automation layer for navigation, clicking, typing, scraping, screenshots, DOM inspection, and JS evaluation.
- Skills and plugins wire the same contract into integrations such as OpenAI Codex, Claude Code, Gemini CLI, Hermes, and OpenClaw.

## MCP Security

- `clawctl mcp` and the packaged `clawbrowser-mcp` server are local stdio only, not network daemons.
- It exposes lifecycle/session tools and returns the local CDP endpoint; treat that endpoint as sensitive.
- Do not expose CDP on the network or publish the Docker port externally unless you explicitly understand the risk.
- Do not put API keys into MCP config, agent config, shell rc files, or logs.
- Use the official `clawbrowser/clawbrowser` GitHub repository and install command only.

## Install

Use the assembled release archive. It contains generated `bin/clawctl`; the raw source checkout does not. Download the archive first, then run the bundled `clawctl install`.

```bash
# macOS Apple Silicon
curl -fsSLO https://github.com/clawbrowser/clawbrowser/releases/latest/download/clawbrowser-macos-arm64.tar.gz

# Linux x64
curl -fsSLO https://github.com/clawbrowser/clawbrowser/releases/latest/download/clawbrowser-linux-x64.tar.gz

# Linux arm64 / aarch64
curl -fsSLO https://github.com/clawbrowser/clawbrowser/releases/latest/download/clawbrowser-linux-arm64.tar.gz
```

```bash
tar -xzf clawbrowser-<platform>.tar.gz
cd clawbrowser-<platform>
./clawctl install --prompt-api-key auto
```

Use `auto` unless you need a specific target such as `codex`, `claude`,
`gemini`, `hermes`, or `openclaw`. Run `./clawctl install <target>` again if
you need more than one target.

Do not use `npx` as the primary install path for agent workflows. The expected bootstrap is the release archive plus bundled `clawctl install`.

The `scripts/install.sh` helper is an internal release-bundle installer invoked
by `clawctl install`. Do not run the raw script from GitHub or a source checkout.

Hermes installs into `~/.hermes/plugins/clawbrowser` and enables MCP as
`clawctl mcp`. The packaged `clawbrowser-mcp` server remains for
compatibility.

See [INSTALL.md](./INSTALL.md) for exact install, auth, and verify steps.

## Installation Paths (Priority Order)

### 1) Linux Portable Runtime (Default for VPS/Servers/Containers)

Use this by default on Linux servers, CI-like environments, and restricted containers.

- No host Docker access required.
- No Docker CLI/daemon/socket required at runtime.
- No `docker-compose`, `sudo`, or `apt` required for runtime use.
- No physical display required.
- Runs full headful Clawbrowser under bundled Xvfb.
- Installer/runtime uses the portable runtime tarball and launches Xvfb + Clawbrowser as local child processes in the current environment.
- Does not require creating a sidecar container.
- Exposes a local CDP endpoint consumed by `clawctl`.
- Docker is not required for portable install or portable start.

```bash
clawctl install
clawctl start
clawctl endpoint
```

Example with explicit session and URL:

```bash
./clawctl install --prompt-api-key auto
clawctl start --session work --url https://example.com --json
clawctl endpoint --session work --json
```

You can force portable backend explicitly:

```bash
clawbrowser start --backend portable --session work -- clawbrowser://verify/
```

### 2) macOS Native App Runtime (Default on macOS)

Use the native `Clawbrowser.app` runtime on macOS (including Mac mini setups). This path uses a normal macOS GUI session through WindowServer.

- Xvfb is Linux-only and is not a macOS runtime path.
- Headless Mac mini setups still need an operator-provisioned GUI session (for example a connected display, dummy HDMI adapter, Screen Sharing session, or equivalent).

### 3) Docker Backend (Optional, Explicit, Operator-Managed)

Docker remains supported when operators intentionally want a Dockerized runtime and control the host.

- This is optional and not the default Linux/container path.
- Restricted containers usually cannot start Docker by themselves.
- Running Docker from inside another container requires privileged/socket setup and is not recommended for untrusted agents.
- Docker-in-Docker or socket-mount access is privileged and security-sensitive.
- Sidecar deployments remain valid as operator-managed infrastructure, not agent self-bootstrap.
- Release CI/build pipelines may use Docker internally, but end-user portable install/runtime does not require Docker.

### 4) Existing CDP Endpoint (Advanced)

Use this when your environment already provisions a browser and CDP endpoint (escape hatch mode).

- `clawctl` does not start a new managed browser in this mode.
- You pass the endpoint explicitly for each command.
- Browser lifecycle, isolation, and host security controls are owned by external infrastructure.

```bash
clawctl --cdp http://127.0.0.1:9222 tabs list --json
clawctl --cdp http://127.0.0.1:9222 verify --json
```

## Basic Workflow

```bash
clawctl start --session work --url https://example.com --json
clawctl endpoint --session work --json
clawctl rotate --session work --json
clawctl rotate --session work --url clawbrowser://verify/ --json
clawctl stop --session work --json
```

`clawbrowser://verify` is the explicit verify page. Use it when fingerprint, proxy, or geo proof matters, or when you are debugging browser-quality issues. The session is ready the moment `start` prints a live CDP endpoint. Reuse the same session name to reattach, use a new one for a separate profile, use `clawctl sessions list` for live sessions, use `clawctl list` for cached-profile inspection, and use `clawctl stop` for cleanup.

Do not bypass the managed launcher for agent tasks. On macOS, the launcher may use Clawbrowser.app as the native runtime.

## Runtime Modes

- **Host mode** — macOS or Linux with a display. Uses `Clawbrowser.app` when present.
- **Portable Xvfb mode (recommended on Linux servers/containers)** — starts the bundled portable Linux runtime with virtual display support. This is the default Linux path and does not require Docker at runtime.
- **Docker backend mode (optional/operator-managed)** — for environments that intentionally standardize on host-managed container runtime. Agents connect over `clawctl --cdp ...` when operators provide that endpoint.
- **Existing CDP endpoint mode (advanced)** — use `clawctl --cdp ...` to drive a browser provisioned elsewhere.

Use `clawbrowser start --backend portable` to force the portable runtime.

In launcher `auto` mode, Clawbrowser prefers native app runtime on macOS and portable runtime on Linux. In Linux containers, Docker backend fallback is gated and only considered when `CLAWBROWSER_ALLOW_DOCKER_BACKEND=1` is set.

See [INSTALL.md](./INSTALL.md#setup-modes-priority-order) for setup details and [INSTALL.md](./INSTALL.md#3-optional-docker-backend-explicit-operator-managed) for optional Docker backend commands.
