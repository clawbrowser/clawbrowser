"""Tool handlers -- execute Clawbrowser CLI commands."""

import json
import os
import shutil
import subprocess


def _find_clawbrowser() -> str:
    """Locate the clawbrowser CLI binary."""
    explicit = os.environ.get("CLAWBROWSER_BIN")
    if explicit and os.path.isfile(explicit):
        return explicit

    found = shutil.which("clawbrowser")
    if found:
        return found

    for candidate in (
        os.path.expanduser("~/.local/bin/clawbrowser"),
        os.path.expanduser("~/.clawbrowser/bin/clawbrowser"),
    ):
        if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
            return candidate

    return "clawbrowser"


def _run(args: list[str], timeout: int = 30) -> dict:
    """Run a clawbrowser CLI command and return structured output."""
    binary = _find_clawbrowser()
    cmd = [binary] + args
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        output = result.stdout.strip()
        error = result.stderr.strip()
        if result.returncode == 0:
            return {"status": "ok", "output": output}
        return {
            "status": "error",
            "exit_code": result.returncode,
            "output": output,
            "error": error,
        }
    except FileNotFoundError:
        return {
            "status": "error",
            "error": (
                "clawbrowser CLI not found. Install with: "
                "curl -fsSL https://raw.githubusercontent.com/clawbrowser/"
                "clawbrowser/main/scripts/install.sh | bash -s -- all"
            ),
        }
    except subprocess.TimeoutExpired:
        return {"status": "error", "error": f"Command timed out after {timeout}s"}
    except Exception as exc:
        return {"status": "error", "error": str(exc)}


def clawbrowser_start(args: dict, **kwargs) -> str:
    """Start a browser session, optionally opening a URL."""
    session = args.get("session", "").strip()
    if not session:
        return json.dumps({"error": "session name is required"})

    cmd = ["start", "--session", session]
    url = args.get("url", "").strip()
    if url:
        cmd += ["--", url]

    result = _run(cmd, timeout=60)
    return json.dumps(result)


def clawbrowser_endpoint(args: dict, **kwargs) -> str:
    """Get the CDP endpoint for a running session."""
    session = args.get("session", "").strip()
    if not session:
        return json.dumps({"error": "session name is required"})

    result = _run(["endpoint", "--session", session])
    return json.dumps(result)


def clawbrowser_rotate(args: dict, **kwargs) -> str:
    """Rotate identity for a running session."""
    session = args.get("session", "").strip()
    if not session:
        return json.dumps({"error": "session name is required"})

    result = _run(["rotate", "--session", session])
    return json.dumps(result)


def clawbrowser_stop(args: dict, **kwargs) -> str:
    """Stop a session and clean up resources."""
    session = args.get("session", "").strip()
    if not session:
        return json.dumps({"error": "session name is required"})

    result = _run(["stop", "--session", session])
    return json.dumps(result)


def clawbrowser_status(args: dict, **kwargs) -> str:
    """Check the status of a session."""
    session = args.get("session", "").strip()
    if not session:
        return json.dumps({"error": "session name is required"})

    result = _run(["status", "--session", session])
    return json.dumps(result)
