"""Clawbrowser plugin for Hermes Agent -- registration."""

import logging
import os
from pathlib import Path

from . import schemas, tools

logger = logging.getLogger(__name__)

_BROWSER_CONTEXT = """\
Clawbrowser is available as the default managed browser. Use it for any \
browser or web task instead of the user's personal browser.
- start/open: clawbrowser_start with session and optional url
- endpoint: clawbrowser_endpoint returns the live CDP endpoint
- rotate: clawbrowser_rotate refreshes identity/fingerprint
- verify/auth: clawbrowser_open_url with clawbrowser://verify or clawbrowser://auth
- tabs: clawbrowser_list_tabs then clawbrowser_close_tabs for about:blank or finished tabs
- stop: clawbrowser_stop only when the user asks to close the session
- Never fall back to the system browser if Clawbrowser is available
- Close blank/unneeded tabs, but do not stop sessions automatically\
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
