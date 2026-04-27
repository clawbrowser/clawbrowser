"""Clawbrowser plugin for Hermes Agent -- registration."""

import logging
import os
from pathlib import Path

from . import schemas, tools

logger = logging.getLogger(__name__)

_BROWSER_CONTEXT = """\
Clawbrowser is the managed browser for web tasks. Use clawbrowser_* tools or
Clawbrowser MCP tools for lifecycle and identity, then use the current CDP
endpoint for page automation. Do not launch Chrome, Chromium, the system
browser directly for agent tasks. On macOS, Clawbrowser.app may be the
native runtime used under the hood, but agents should not invoke it
directly.
- Managed sessions for agent tasks are expected to run in fingerprint/proxy mode.
- Start/reuse: clawbrowser_start with a session and optional url. After start,
  reattach, restart, or rotate, call clawbrowser_endpoint again and do not
  persist old ws://127.0.0.1/... endpoints.
- Fresh identity: clawbrowser_rotate, then call clawbrowser_endpoint again
  before further automation.
- Verify/auth: use clawbrowser://verify/ inside the managed session when
  fingerprint/proxy/geo proof matters; do not rely on external fingerprint
  checking sites unless the user explicitly asks.
- Tabs: close blank or finished tabs; stop only when the user asks.\
- Browser API keys belong in browser-managed config.json. Resolve config
  paths before writing; do not pass unresolved shell-expression paths to
  file-write tools. They may create literal workspace paths instead of the
  real config file.\
"""

_session_log: list[dict] = []


def _check_clawbrowser_available() -> bool:
    """Return True if the clawbrowser CLI is reachable."""
    import shutil

    for candidate in (
        str(Path(__file__).resolve().parent / "clawbrowser"),
        os.path.expanduser("~/.local/bin/clawbrowser"),
        os.path.expanduser("~/.clawbrowser/bin/clawbrowser"),
    ):
        if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
            return True
    if shutil.which("clawbrowser"):
        return True
    return False


def _on_pre_llm_call(session_id, user_message, is_first_turn, **kwargs):
    """Inject Clawbrowser context on the first turn of each session."""
    if is_first_turn:
        return {"context": _BROWSER_CONTEXT}
    return None


def _on_post_tool_call(tool_name, args, result, task_id, **kwargs):
    """Track Clawbrowser tool calls for diagnostics."""
    if tool_name.startswith("clawbrowser_"):
        entry = {
            "tool": tool_name,
            "session": task_id,
            "browser_session": args.get("session", ""),
        }
        _session_log.append(entry)
        if len(_session_log) > 200:
            _session_log.pop(0)
        logger.debug(
            "clawbrowser tool: %s session=%s browser_session=%s",
            tool_name,
            task_id,
            args.get("session", ""),
        )


def register(ctx):
    """Wire Clawbrowser tools, hooks, and skills into Hermes."""

    ctx.register_tool(
        name="clawbrowser_start",
        toolset="clawbrowser",
        schema=schemas.CLAWBROWSER_START,
        handler=tools.clawbrowser_start,
        check_fn=_check_clawbrowser_available,
    )
    ctx.register_tool(
        name="clawbrowser_endpoint",
        toolset="clawbrowser",
        schema=schemas.CLAWBROWSER_ENDPOINT,
        handler=tools.clawbrowser_endpoint,
        check_fn=_check_clawbrowser_available,
    )
    ctx.register_tool(
        name="clawbrowser_rotate",
        toolset="clawbrowser",
        schema=schemas.CLAWBROWSER_ROTATE,
        handler=tools.clawbrowser_rotate,
        check_fn=_check_clawbrowser_available,
    )
    ctx.register_tool(
        name="clawbrowser_open_url",
        toolset="clawbrowser",
        schema=schemas.CLAWBROWSER_OPEN_URL,
        handler=tools.clawbrowser_open_url,
        check_fn=_check_clawbrowser_available,
    )
    ctx.register_tool(
        name="clawbrowser_list_tabs",
        toolset="clawbrowser",
        schema=schemas.CLAWBROWSER_LIST_TABS,
        handler=tools.clawbrowser_list_tabs,
        check_fn=_check_clawbrowser_available,
    )
    ctx.register_tool(
        name="clawbrowser_close_tabs",
        toolset="clawbrowser",
        schema=schemas.CLAWBROWSER_CLOSE_TABS,
        handler=tools.clawbrowser_close_tabs,
        check_fn=_check_clawbrowser_available,
    )
    ctx.register_tool(
        name="clawbrowser_stop",
        toolset="clawbrowser",
        schema=schemas.CLAWBROWSER_STOP,
        handler=tools.clawbrowser_stop,
        check_fn=_check_clawbrowser_available,
    )
    ctx.register_tool(
        name="clawbrowser_status",
        toolset="clawbrowser",
        schema=schemas.CLAWBROWSER_STATUS,
        handler=tools.clawbrowser_status,
        check_fn=_check_clawbrowser_available,
    )

    ctx.register_hook("pre_llm_call", _on_pre_llm_call)
    ctx.register_hook("post_tool_call", _on_post_tool_call)

    # Register bundled skills
    skills_dir = Path(__file__).parent / "skills"
    if not skills_dir.exists():
        return
    for child in sorted(skills_dir.iterdir()):
        skill_md = child / "SKILL.md"
        if child.is_dir() and skill_md.exists():
            ctx.register_skill(child.name, skill_md)
            logger.debug("Registered skill: clawbrowser:%s", child.name)
