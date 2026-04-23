# Clawbrowser

Clawbrowser is a managed browser runtime for AI agents — fingerprints,
proxies, rotating identities, and a local CDP endpoint out of the box.
The agent never touches the human's personal browser profile.

It works with any agent harness that can talk to a CDP endpoint or an
MCP server. This repo ships ready-made integrations for OpenAI Codex,
Gemini CLI, Hermes, Claude Desktop/Claude Code, and other
plugin-capable agents.

## How it works

Clawbrowser ships in a few surfaces depending on the agent:

- A **`clawbrowser` CLI** that starts managed CDP sessions with isolated
  config dirs, handles native-app → container fallback, and rotates
  identity on demand.
- A **skill** (`SKILL.md`) that explains when and how to reach for the
  browser — loaded automatically by agents that support skills.
- A **`clawbrowser-mcp` MCP server** with tabs, navigation, and tab
  cleanup controls.
- **Agent integrations** (plugins/extensions) that wire the runtime and
  MCP server into specific harnesses (OpenAI Codex, Gemini CLI, Hermes,
  Claude Desktop, Claude Code, and more).

The browser-managed `config.json` stores the long-lived API key. Some
integrations expose an optional UI field to set that key once (for
example the Claude Desktop extension), but the runtime always reuses the
same `config.json` afterward.

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
enables it in `~/.hermes/config.yaml`. Five tools land immediately:
`clawbrowser_start`, `clawbrowser_endpoint`, `clawbrowser_rotate`,
`clawbrowser_stop`, `clawbrowser_status`.

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

### Claude Desktop extension (.mcpb)

Claude Desktop supports installing `.mcpb` extensions. This packaging is
specific to Claude Desktop; other harnesses should use the shell
installer targets above. Download
`clawbrowser-desktop-extension.mcpb` from the Releases page and install
it in Claude Desktop by double-clicking the file or using
Settings → Extensions → Install Extension.

- Direct download: [clawbrowser-desktop-extension.mcpb](https://github.com/clawbrowser/clawbrowser/releases/latest/download/clawbrowser-desktop-extension.mcpb)
- All releases: [github.com/clawbrowser/clawbrowser/releases](https://github.com/clawbrowser/clawbrowser/releases)

From a local checkout you can build the bundle yourself:

```bash
python3 scripts/build_mcpb.py --output clawbrowser-desktop-extension.mcpb
```

## Basic workflow

```bash
clawbrowser start    --session work -- https://example.com
clawbrowser endpoint --session work
clawbrowser rotate   --session work      # fresh identity / fingerprint
clawbrowser stop     --session work
```

The session is ready the moment `start` prints a live CDP endpoint —
agents should continue immediately. Use separate session names per
agent.

## Runtime modes

- **Host mode** — macOS or Linux with a display. Uses
  `Clawbrowser.app` when present.
- **Container mode** — VPS, server, SSH-only, headless. Uses
  `docker.io/clawbrowser/clawbrowser:latest` via any OCI runtime
  (Docker, Podman, nerdctl, containerd).

In `auto` mode the launcher prefers the native app on macOS and falls
back to container automatically if native CDP startup fails.

## What's inside

```
AGENTS.md                  Agent-level instructions (CLAUDE.md, GEMINI.md symlink here)
SKILL.md                   Canonical skill — how to use the browser
INSTALL.md                 Full install & verification commands
claude-desktop-extension/   Claude Desktop extension source
  manifest.json              MCPB manifest
  icon.png                   Extension icon
  server/index.js            Bundle entry point
bin/clawbrowser            CLI entry point
bin/clawbrowser-mcp        MCP server entry point
.claude-plugin/plugin.json  Claude Code plugin manifest
.codex-plugin/plugin.json   Codex plugin manifest
.hermes-plugin/plugin.yaml  Hermes plugin manifest
skills/clawbrowser/SKILL.md Skill alias used by plugin bundles
scripts/build_mcpb.py       Claude Desktop bundle packer
scripts/install.sh         The one-liner installer
Dockerfile                 Container image definition
```

Each harness's install contract is defined by these manifests:

| Harness       | Manifest                                   |
|---------------|--------------------------------------------|
| OpenAI Codex  | `.codex-plugin/plugin.json`                |
| Gemini CLI    | `gemini-extension.json` (repo root)        |
| Hermes        | `.hermes-plugin/plugin.yaml`               |
| Claude Code   | `.claude-plugin/plugin.json`               |
| Claude Desktop | `claude-desktop-extension/manifest.json`   |

The skill is defined once at repo root (`SKILL.md`). Harness-specific
bundles (for example the Claude Desktop `.mcpb` extension) are assembled
from their surface directories plus the shared launcher scripts at build
time.

## Dashboard + API key

Get an API key at [app.clawbrowser.ai](https://app.clawbrowser.ai).
Some integrations provide a UI field to paste it once (for example the
Claude Desktop extension). The runtime writes it to `config.json` with
`chmod 600` and reuses it forever after. Need to re-auth? Open
`clawbrowser://auth` in the managed browser.
