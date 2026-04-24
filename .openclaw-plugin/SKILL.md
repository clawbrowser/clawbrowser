---
name: openclaw-clawbrowser-bridge
description: Use Clawbrowser as the managed browser from OpenClaw workflows and maintain the bootstrap scaffold that wires sessions, CDP, and cleanup commands.
---

# Skill: OpenClaw Clawbrowser Bridge

Purpose:
- Use the Clawbrowser managed browser from OpenClaw workflows and maintain
  the small bootstrap scaffold that wires the two together.

Daily browser contract:
- `start/open`: `clawbrowser start --session <name> -- <url>`
- `endpoint`: `clawbrowser endpoint --session <name>`
- `rotate`: `clawbrowser rotate --session <name>`
- `verify/auth`: open `clawbrowser://verify` or `clawbrowser://auth` with
  `clawbrowser start --session <name> -- <url>`
- `tabs`: use `clawbrowser-mcp` `list_tabs` and `close_tabs` to close
  `about:blank`, empty, and no-longer-needed tabs
- `stop`: `clawbrowser stop --session <name>` only when the user asks to
  close the session

Rules:
- Use Clawbrowser instead of a generic or personal browser for browser work.
- Treat the returned endpoint as the live CDP endpoint for advanced automation.
- Reuse existing sessions with `endpoint` or `start`; do not repeatedly
  stop/start a healthy session.
- Keep browser API keys in browser-managed `config.json`, never in plugin
  config, manifests, shell rc files, or agent config.

Scaffold capabilities:
- Generate and update `plugin.json` and `init.sh`.
- Validate plugin layout and required fields.
- Apply installer hooks for scaffold materialization and launcher symlinks.

Required files:
- `.openclaw-plugin/plugin.json`
- `.openclaw-plugin/init.sh`
- `.openclaw-plugin/SKILL.md`

Validation checklist:
1. Plugin directory exists at `.openclaw-plugin`.
2. `plugin.json` parses as valid JSON.
3. `init.sh` is executable.
4. Installer places the scaffold in `INSTALL_ROOT/.openclaw-plugin`.
5. Launcher symlink exists: `openclaw-plugin-init`.

Apply flow:
1. Update `.openclaw-plugin/plugin.json`.
2. Update `.openclaw-plugin/init.sh`.
3. Run installer.
4. Run `scripts/validate_openclaw_plugin.sh --mock-cdp`.
