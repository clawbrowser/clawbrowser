# Clawbrowser

Clawbrowser is a managed browser runtime for AI agents. Lifecycle and identity come from the CLI/MCP layer; CDP is the page automation layer. Managed sessions for agent tasks are expected to run in fingerprint/proxy mode, and the browser never touches the human's personal browser profile.

Full contract: [AGENTS.md](./AGENTS.md)

## How It Works

- `clawctl` creates or reattaches managed sessions and exposes a local CDP endpoint.
- `clawctl mcp` provides the same lifecycle and identity controls through MCP.
- Release archives install `clawctl`, `clawbrowser`, and `clawbrowser-mcp`.
- CDP is the page automation layer for navigation, clicking, typing, scraping, screenshots, DOM inspection, and JS evaluation.
- Skills and plugins wire the same contract into agent tools such as OpenAI Codex, Claude Code, Gemini CLI, Hermes, and OpenClaw.

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
- **Container mode** — VPS, server, SSH, or no physical display. Uses `docker.io/clawbrowser/clawbrowser:latest` via Docker or a Docker-compatible OCI CLI configured through `CLAWBROWSER_DOCKER_BIN`. This runs full Clawbrowser with a virtual Linux display and exposes CDP; it is not Chrome headless mode.

In launcher `auto` mode, the app prefers the native app on macOS and falls back to container automatically if native CDP startup fails.
