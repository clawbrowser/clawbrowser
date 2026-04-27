# Clawbrowser

Clawbrowser is a managed browser runtime for AI agents. Lifecycle and identity come from the CLI/MCP layer; CDP is the page automation layer. Managed sessions for agent tasks are expected to run in fingerprint/proxy mode, and the browser never touches the human's personal browser profile.

Full contract: [AGENTS.md](./AGENTS.md)

## How It Works

- `clawbrowser` CLI creates or reattaches managed sessions and exposes a local CDP endpoint.
- `clawbrowser-mcp` provides the same lifecycle and identity controls through MCP.
- CDP is the page automation layer for navigation, clicking, typing, scraping, screenshots, DOM inspection, and JS evaluation.
- Skills and plugins wire the same contract into agent tools such as OpenAI Codex, Gemini CLI, Hermes, and Claude Code.

## Install

Use the installer `auto` target if you want it to pick the right target for you.

### Shell Installer

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- <target>
```

Targets:

- Installer `auto` (recommended)

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- auto
```

`auto` chooses `hermes`, `codex`, `claude`, or `gemini`; it does not install every target.

- OpenAI Codex (CLI + plugin-capable surfaces)

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- codex
```

Drops the plugin into `~/.codex/plugins/clawbrowser` and marks it `INSTALLED_BY_DEFAULT` in `~/.agents/plugins/marketplace.json`.

- Gemini CLI

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- gemini
```

- Hermes Agent

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- hermes
```

Installs the Hermes plugin into `~/.hermes/plugins/clawbrowser` and enables it in `~/.hermes/config.yaml`. It also configures the portable `clawbrowser-mcp` MCP server. Native tools land immediately: `clawbrowser_start`, `clawbrowser_endpoint`, `clawbrowser_rotate`, `clawbrowser_open_url`, `clawbrowser_list_tabs`, `clawbrowser_close_tabs`, `clawbrowser_stop`, `clawbrowser_status`.

- Claude Code

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- claude
```

- OpenClaw compatibility bootstrap

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- openclaw
```

Installs the legacy OpenClaw compatibility scaffold for historical bootstrap paths. Normal agent workflows still use `clawbrowser` and `clawbrowser-mcp`.

- Install another target

Run the installer once per target if you need more than one of the listed targets.

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- <target>
```

Or, via npx:

```bash
npx --yes github:clawbrowser/clawbrowser <target>
```

See [INSTALL.md](./INSTALL.md) for exact install, auth, and verify steps.

## Basic Workflow

```bash
clawbrowser start --session work -- https://example.com
clawbrowser endpoint --session work
clawbrowser rotate --session work
clawbrowser rotate --session work -- clawbrowser://verify
clawbrowser stop --session work
```

`clawbrowser://verify` is the explicit verify page. Use it when fingerprint, proxy, or geo proof matters, or when you are debugging browser-quality issues. The session is ready the moment `start` prints a live CDP endpoint. Reuse the same session name to reattach, use a new one for a separate profile, and use `clawbrowser status`, `clawbrowser list`, and `clawbrowser stop` for cleanup and cached-profile inspection.

Do not bypass the managed launcher for agent tasks. On macOS, the launcher may use Clawbrowser.app as the native runtime.

## Runtime Modes

- **Host mode** — macOS or Linux with a display. Uses `Clawbrowser.app` when present.
- **Container mode** — VPS, server, SSH, or no physical display. Uses `docker.io/clawbrowser/clawbrowser:latest` via Docker or a Docker-compatible OCI CLI configured through `CLAWBROWSER_DOCKER_BIN`. This runs full Clawbrowser with a virtual Linux display and exposes CDP; it is not Chrome headless mode.

In launcher `auto` mode, the app prefers the native app on macOS and falls back to container automatically if native CDP startup fails.
