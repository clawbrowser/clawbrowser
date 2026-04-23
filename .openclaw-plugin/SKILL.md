---
name: openclaw-plugin-manager
description: Create, validate, and apply OpenClaw plugin configurations for Clawbrowser integration.
---

# Skill: OpenClaw Plugin Manager

Purpose:
- Manage OpenClaw plugin scaffolding and config for Clawbrowser bridge workflows.

Capabilities:
- Generate and update `plugin.json` and `init.sh`.
- Validate plugin layout and required fields.
- Apply installer hooks for plugin materialization and launcher symlinks.
- Enforce security rule: browser API keys remain in browser-managed `config.json`.

Required files:
- `.openclaw-plugin/plugin.json`
- `.openclaw-plugin/init.sh`
- `.openclaw-plugin/SKILL.md`

Validation checklist:
1. Plugin directory exists at `.openclaw-plugin`.
2. `plugin.json` parses as valid JSON.
3. `init.sh` is executable.
4. Installer places plugin in `INSTALL_ROOT/.openclaw-plugin`.
5. Launcher symlink exists: `openclaw-plugin-init`.

Apply flow:
1. Update `.openclaw-plugin/plugin.json`.
2. Update `.openclaw-plugin/init.sh`.
3. Run installer.
4. Run `scripts/validate_openclaw_plugin.sh --mock-cdp`.
