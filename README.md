# Clawbrowser

Clawbrowser is a managed browser runtime for AI agents — fingerprints,
proxies, rotating identities, and a local CDP endpoint out of the box.
The agent never touches the human's personal browser profile.

It works with any agent harness that can talk to a CDP endpoint or an
MCP server. This repo ships ready-made integrations for OpenAI Codex,
Gemini CLI, Hermes, Claude Code, and other plugin-capable agents.

## How it works

Clawbrowser ships in a few surfaces depending on the agent:

- A **`clawbrowser` CLI** that starts managed CDP sessions with isolated
  config dirs, handles native-app → container fallback, and rotates
  identity on demand.
- A **skill** (`SKILL.md`) that explains when and how to reach for the
  browser — loaded automatically by agents that support skills.
- A **`clawbrowser-mcp` MCP server** with session start/open, endpoint,
  rotation, and tab cleanup controls.
- **Agent integrations** (plugins/extensions) that wire the runtime and
  MCP server into specific harnesses (OpenAI Codex, Gemini CLI, Hermes,
  Claude Code, and more).

## Install

Pick the integration that matches your agent harness.

### Shell installer (plugins + CLI)

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- <target>
```

Targets:

- OpenAI Codex (CLI + plugin-capable surfaces)

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- codex
```

Drops the plugin into `~/.codex/plugins/clawbrowser` and marks it
`INSTALLED_BY_DEFAULT` in `~/.agents/plugins/marketplace.json`.

- Gemini CLI

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- gemini
```

- Hermes Agent

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- hermes
```

Installs the Hermes plugin into `~/.hermes/plugins/clawbrowser` and
enables it in `~/.hermes/config.yaml`. Eight tools land immediately:
`clawbrowser_start`, `clawbrowser_endpoint`, `clawbrowser_rotate`,
`clawbrowser_open_url`, `clawbrowser_list_tabs`,
`clawbrowser_close_tabs`, `clawbrowser_stop`, `clawbrowser_status`.

- Claude Code

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- claude
```

- Cursor, other agents, multi-target

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- all
```

Or, via npx (no clone needed):

```bash
npx --yes github:clawbrowser/clawbrowser <target>
```

See [INSTALL.md](./INSTALL.md) for verification commands and config
paths per mode.

## Basic workflow

```bash
clawbrowser start    --session work -- https://example.com
clawbrowser endpoint --session work
clawbrowser rotate   --session work      # fresh identity / fingerprint
clawbrowser start    --session work -- clawbrowser://verify
clawbrowser stop     --session work
```

The session is ready the moment `start` prints a live CDP endpoint —
agents should continue immediately. Use separate session names per
agent. Agents should close `about:blank`, empty, and no-longer-needed
tabs, but should not stop sessions automatically.

## Runtime modes

- **Host mode** — macOS or Linux with a display. Uses
  `Clawbrowser.app` when present.
- **Container mode** — VPS, server, SSH-only, headless. Uses
  `docker.io/clawbrowser/clawbrowser:latest` via Docker or a
  Docker-compatible OCI CLI configured through `CLAWBROWSER_DOCKER_BIN`.

In `auto` mode the launcher prefers the native app on macOS and falls
back to container automatically if native CDP startup fails.
