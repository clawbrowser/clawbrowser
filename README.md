# Clawbrowser

Browser built for AI agents. Fingerprints, proxies and rotating identities out of the box.

## Install

Supported surfaces:

- Claude Code and Claude Desktop
- Cursor
- Codex app and plugin-capable Codex surfaces
- Gemini CLI
- Other agents

### npx install

Install directly from the GitHub repo without cloning:

```bash
npx --yes github:clawbrowser/clawbrowser <target>
```

Targets:

- `claude` for Claude Code and Claude Desktop (`claude-desktop` is an alias)
- `codex` for Codex
- `gemini` for Gemini CLI
- `all` for other agents or multi-target installs

### Claude Code and Claude Desktop

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- claude
```

### Cursor

Use the `all` target for the shared bundle and every supported integration.

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- all
```

### Codex app and plugin-capable Codex surfaces

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- codex
```

Codex copies the plugin into `~/.codex/plugins/clawbrowser` and marks it
`INSTALLED_BY_DEFAULT` in `~/.agents/plugins/marketplace.json`. No manual
`/plugin marketplace` step is needed.

### Gemini CLI

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- gemini
```

### Other agents

```bash
curl -fsSL https://raw.githubusercontent.com/clawbrowser/clawbrowser/main/scripts/install.sh | bash -s -- all
```

## Runtime modes

- **Host mode** — desktop/macOS or Linux with a display. The launcher uses
  `Clawbrowser.app` when present.
- **Container mode** — VPS, server, SSH-only, headless. Use the
  `docker.io/clawbrowser/clawbrowser:latest` OCI image with any OCI
  runtime (Docker, Podman, nerdctl, containerd, etc.); the
  `clawbrowser-config` named volume keeps the config across restarts.

Default to container mode when there is no display. In `auto` mode the
launcher also falls back native→container on its own.

## What gets installed

- `browser-user` agent that routes all browser and web tasks through Clawbrowser
- `clawbrowser` skill with usage guidance for start, rotate, stop, and verify
- `clawbrowser-mcp` MCP server for CDP session management

## CLI usage

```bash
clawbrowser start --session <name> -- <url>    # start and open a URL
clawbrowser endpoint --session <name>          # print live CDP endpoint
clawbrowser rotate --session <name>            # fresh identity/fingerprint
clawbrowser stop --session <name>              # clean up
```