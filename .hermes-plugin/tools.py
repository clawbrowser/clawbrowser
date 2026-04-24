"""Tool handlers -- execute Clawbrowser CLI commands."""

import json
import os
from pathlib import Path
import shutil
import subprocess
from urllib import error as urllib_error
from urllib import parse as urllib_parse
from urllib import request as urllib_request


def _find_clawbrowser() -> str:
    """Locate the clawbrowser CLI binary."""
    explicit = os.environ.get("CLAWBROWSER_BIN")
    if explicit and os.path.isfile(explicit):
        return explicit

    for candidate in (
        Path(__file__).resolve().parent / "clawbrowser",
        os.path.expanduser("~/.local/bin/clawbrowser"),
        os.path.expanduser("~/.clawbrowser/bin/clawbrowser"),
    ):
        candidate = str(candidate)
        if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
            return candidate

    found = shutil.which("clawbrowser")
    if found:
        return found

    return "clawbrowser"


def _saved_config_path() -> Path:
    config_home = os.environ.get("XDG_CONFIG_HOME")
    if config_home:
        return Path(config_home) / "clawbrowser" / "config.json"
    return Path.home() / ".config" / "clawbrowser" / "config.json"


def _saved_config_has_api_key() -> bool:
    try:
        payload = json.loads(_saved_config_path().read_text(encoding="utf-8"))
    except Exception:
        return False

    api_key = payload.get("api_key")
    return isinstance(api_key, str) and bool(api_key.strip())


def _bootstrap_error() -> dict:
    return {
        "status": "error",
        "error": (
            f"Clawbrowser can reuse the saved browser config at {_saved_config_path()}. "
            "If it is missing, ask the user once for the real API key from "
            "https://app.clawbrowser.ai, pass it as a bootstrap-only api_key "
            "tool argument, and let the launcher write the browser-managed "
            "config.json with mode 600. For reauth, open clawbrowser://auth in "
            "Clawbrowser."
        ),
    }


def _run(args: list[str], timeout: int = 30, api_key=None, image=None) -> dict:
    """Run a clawbrowser CLI command and return structured output."""
    binary = _find_clawbrowser()
    cmd = [binary] + args
    env = os.environ.copy()
    if api_key:
        env["CLAWBROWSER_API_KEY"] = api_key
    if image:
        env["CLAWBROWSER_IMAGE"] = image

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            stdin=subprocess.DEVNULL,
            env=env,
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
                "clawbrowser CLI not found. Install Clawbrowser for Hermes "
                "or set CLAWBROWSER_BIN to the launcher path."
            ),
        }
    except subprocess.TimeoutExpired:
        return {"status": "error", "error": f"Command timed out after {timeout}s"}
    except Exception as exc:
        return {"status": "error", "error": str(exc)}


def _truthy(value) -> bool:
    if isinstance(value, bool):
        return value
    if value is None:
        return False
    return str(value).strip().lower() not in {"", "0", "false", "no", "off"}


def _append_browser_args(cmd: list[str], args: dict) -> None:
    browser_args = []

    fingerprint = args.get("fingerprint")
    if _truthy(fingerprint):
        browser_args.append("--fingerprint")
        if not isinstance(fingerprint, bool) and str(fingerprint).strip().lower() not in {"1", "true", "yes", "on"}:
            browser_args.append(str(fingerprint))
    if args.get("regenerate"):
        browser_args.append("--regenerate")
    if args.get("verify_automation"):
        browser_args.append("--verify-automation")
    if args.get("country"):
        browser_args.append(f"--country={args['country']}")
    if args.get("city"):
        browser_args.append(f"--city={args['city']}")
    if args.get("connection_type"):
        browser_args.append(f"--connection-type={args['connection_type']}")

    url = str(args.get("url") or "").strip()
    if browser_args or url:
        cmd.append("--")
        cmd.extend(browser_args)
        if url:
            cmd.append(url)


def _normalize_endpoint(endpoint: str) -> str:
    endpoint = endpoint.strip().rstrip("/")
    parsed = urllib_parse.urlparse(endpoint)
    if parsed.scheme in {"ws", "wss"}:
        scheme = "http" if parsed.scheme == "ws" else "https"
        return f"{scheme}://{parsed.netloc}"
    return endpoint


def _endpoint_for_session(session: str):
    result = _run(["endpoint", "--session", session])
    if result.get("status") != "ok":
        return None, result

    endpoint = str(result.get("output") or "").splitlines()[-1].strip()
    if not endpoint:
        return None, {"status": "error", "error": "launcher did not return a CDP endpoint"}
    return endpoint, None


def _fetch_json(url: str):
    with urllib_request.urlopen(url, timeout=2) as response:
        return json.load(response)


def _fetch_page_targets(endpoint: str) -> list[dict]:
    payload = _fetch_json(f"{_normalize_endpoint(endpoint)}/json/list")
    if not isinstance(payload, list):
        raise ValueError("unexpected /json/list payload")
    return [target for target in payload if target.get("type") == "page"]


def _open_page_target(endpoint: str, url: str) -> dict:
    target_url = (
        f"{_normalize_endpoint(endpoint)}/json/new?"
        f"{urllib_parse.quote(url, safe='')}"
    )
    request = urllib_request.Request(target_url, method="PUT")
    try:
        with urllib_request.urlopen(request, timeout=2) as response:
            return json.load(response)
    except urllib_error.HTTPError as exc:
        if exc.code != 405:
            raise
        with urllib_request.urlopen(target_url, timeout=2) as response:
            return json.load(response)


def _close_page_target(endpoint: str, target_id: str) -> str:
    url = (
        f"{_normalize_endpoint(endpoint)}/json/close/"
        f"{urllib_parse.quote(str(target_id), safe='')}"
    )
    with urllib_request.urlopen(url, timeout=2) as response:
        return response.read().decode("utf-8", errors="replace").strip()


def clawbrowser_start(args: dict, **kwargs) -> str:
    """Start a browser session, optionally opening a URL."""
    session = args.get("session", "").strip()
    if not session:
        return json.dumps({"error": "session name is required"})

    api_key = args.get("api_key") or os.environ.get("CLAWBROWSER_API_KEY")
    if not api_key and not _saved_config_has_api_key():
        return json.dumps(_bootstrap_error())

    cmd = ["start", "--session", session]
    if args.get("port"):
        cmd.extend(["--port", str(args["port"])])
    if args.get("image"):
        cmd.extend(["--image", str(args["image"])])
    if args.get("verify"):
        cmd.append("--verify")
    _append_browser_args(cmd, args)

    result = _run(cmd, timeout=90, api_key=api_key, image=args.get("image"))
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

    api_key = args.get("api_key") or os.environ.get("CLAWBROWSER_API_KEY")
    if not api_key and not _saved_config_has_api_key():
        return json.dumps(_bootstrap_error())

    cmd = ["rotate", "--session", session]
    if args.get("image"):
        cmd.extend(["--image", str(args["image"])])
    if args.get("verify"):
        cmd.append("--verify")
    _append_browser_args(cmd, args)

    result = _run(cmd, timeout=90, api_key=api_key, image=args.get("image"))
    return json.dumps(result)


def clawbrowser_open_url(args: dict, **kwargs) -> str:
    """Open a URL in a running session via the CDP HTTP endpoint."""
    session = args.get("session", "").strip()
    url = args.get("url", "").strip()
    if not session:
        return json.dumps({"error": "session name is required"})
    if not url:
        return json.dumps({"error": "url is required"})

    endpoint, error = _endpoint_for_session(session)
    if error:
        return json.dumps(error)

    try:
        target = _open_page_target(endpoint, url)
        return json.dumps({"status": "ok", "endpoint": _normalize_endpoint(endpoint), "target": target})
    except Exception as exc:
        return json.dumps({"status": "error", "error": str(exc)})


def clawbrowser_list_tabs(args: dict, **kwargs) -> str:
    """List page tabs in a running session."""
    session = args.get("session", "").strip()
    if not session:
        return json.dumps({"error": "session name is required"})

    endpoint, error = _endpoint_for_session(session)
    if error:
        return json.dumps(error)

    try:
        tabs = _fetch_page_targets(endpoint)
        return json.dumps({"status": "ok", "endpoint": _normalize_endpoint(endpoint), "tabs": tabs})
    except Exception as exc:
        return json.dumps({"status": "error", "error": str(exc)})


def clawbrowser_close_tabs(args: dict, **kwargs) -> str:
    """Close selected page tabs in a running session."""
    session = args.get("session", "").strip()
    if not session:
        return json.dumps({"error": "session name is required"})

    endpoint, error = _endpoint_for_session(session)
    if error:
        return json.dumps(error)

    try:
        targets = _fetch_page_targets(endpoint)
    except Exception as exc:
        return json.dumps({"status": "error", "error": str(exc)})

    requested_ids = [str(target_id) for target_id in (args.get("target_ids") or []) if str(target_id).strip()]
    url_contains = args.get("url_contains")
    title_contains = args.get("title_contains")
    all_pages = bool(args.get("all_pages"))

    if requested_ids:
        selected = [target for target in targets if str(target.get("id")) in requested_ids]
        missing_target_ids = [target_id for target_id in requested_ids if target_id not in {str(target.get("id")) for target in selected}]
    elif url_contains or title_contains or all_pages:
        selected = targets
        missing_target_ids = []
        if url_contains:
            selected = [target for target in selected if url_contains in (target.get("url") or "")]
        if title_contains:
            selected = [target for target in selected if title_contains in (target.get("title") or "")]
    else:
        return json.dumps({"status": "error", "error": "Specify target_ids, url_contains/title_contains, or all_pages=true."})

    if not selected:
        return json.dumps(
            {
                "status": "error",
                "endpoint": _normalize_endpoint(endpoint),
                "closed": [],
                "missing_target_ids": missing_target_ids,
                "errors": [],
                "error": "No matching tabs found.",
            }
        )

    closed = []
    errors = []
    for target in selected:
        target_id = str(target.get("id") or "").strip()
        if not target_id:
            continue
        try:
            closed.append(
                {
                    "id": target_id,
                    "title": target.get("title", ""),
                    "url": target.get("url", ""),
                    "response": _close_page_target(endpoint, target_id),
                }
            )
        except urllib_error.HTTPError as exc:
            errors.append({"id": target_id, "error": f"HTTP {exc.code}"})
        except Exception as exc:
            errors.append({"id": target_id, "error": str(exc)})

    status = "ok" if closed or not errors else "error"
    return json.dumps(
        {
            "status": status,
            "endpoint": _normalize_endpoint(endpoint),
            "closed": closed,
            "missing_target_ids": missing_target_ids,
            "errors": errors,
        }
    )


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
