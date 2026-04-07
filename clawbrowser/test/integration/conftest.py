"""Shared fixtures for clawbrowser integration tests."""

import asyncio
import json
import os
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
from contextlib import asynccontextmanager
from pathlib import Path

import pytest_asyncio
from playwright.async_api import async_playwright

FIXTURE_DIR = Path(__file__).parent.parent / "fixtures"
WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
PROJECT_ROOT = Path(os.environ.get("CLAWBROWSER_PROJECT_DIR", WORKSPACE_ROOT))
MOCK_SERVER_SCRIPT = PROJECT_ROOT / "clawbrowser/test/integration/mock_server.py"
DEFAULT_FINGERPRINTS_FIXTURE_PATH = PROJECT_ROOT / "api/mocks/fingerprints.json"
DEFAULT_PROXY_FIXTURE_PATH = PROJECT_ROOT / "api/mocks/proxy.json"
MISMATCH_PROXY_FIXTURE_PATH = PROJECT_ROOT / "api/mocks/proxy_mismatch.json"
FINGERPRINT_ID = "fp_test"
TEST_API_KEY = "test_api_key_123"
CDP_READY_TIMEOUT_SECONDS = 30
VERIFY_PAGE_URL = "clawbrowser://verify/"
DEFAULT_BROWSER_ARGS = [
    "--no-first-run",
    "--no-default-browser-check",
    "--disable-features=DialMediaRouteProvider",
    "--use-mock-keychain",
    "--headless=new",
    "--window-size=1280,720",
]


def _read_json(path: Path):
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def _config_dir(home_dir: Path) -> Path:
    return home_dir / ".config" / "clawbrowser"


def _resolve_browser_binary() -> str:
    binary = os.environ.get("CLAWBROWSER_BINARY")
    candidates = [
        Path(binary).expanduser() if binary else None,
        WORKSPACE_ROOT / "out/CBFast/Chromium.app/Contents/MacOS/Chromium",
        WORKSPACE_ROOT / "out/Default/chrome",
    ]

    for candidate in candidates:
        if candidate and candidate.exists():
            return str(candidate)

    searched = "\n".join(
        str(candidate) for candidate in candidates if candidate is not None
    )
    raise FileNotFoundError(
        "Could not find a Chromium binary for integration tests. "
        "Set CLAWBROWSER_BINARY or build one of:\n"
        f"{searched}"
    )


def _reserve_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def _seed_profile(config_dir: Path, fixture_name: str):
    fingerprint_path = FIXTURE_DIR / fixture_name
    fingerprint_data = _read_json(fingerprint_path)
    profile_dir = config_dir / "Browser" / FINGERPRINT_ID
    profile_dir.mkdir(parents=True, exist_ok=True)
    (profile_dir / "fingerprint.json").write_text(
        json.dumps(fingerprint_data, indent=2) + "\n",
        encoding="utf-8",
    )
    return fingerprint_data


def _seed_config(config_dir: Path, base_url: str, api_key=None):
    config_dir.mkdir(parents=True, exist_ok=True)
    payload = {
        "api_base_url": base_url,
    }
    if api_key:
        payload["api_key"] = api_key
    (config_dir / "config.json").write_text(
        json.dumps(payload, indent=2) + "\n",
        encoding="utf-8",
    )


def _read_log(log_path: Path) -> str:
    if not log_path.exists():
        return ""
    return log_path.read_text(encoding="utf-8", errors="replace")


def _maybe_dump_log(label: str, log_path: Path):
    if os.environ.get("CLAWBROWSER_DEBUG_LOGS") != "1":
        return
    log_contents = _read_log(log_path)
    if not log_contents:
        return
    print(f"\n--- {label} ---\n{log_contents}", file=sys.stderr, flush=True)


def _wait_for_http_ready(url: str, process: subprocess.Popen, log_path: Path):
    deadline = time.time() + CDP_READY_TIMEOUT_SECONDS
    last_error = None

    while time.time() < deadline:
        if process.poll() is not None:
            raise RuntimeError(
                "Process exited before endpoint became ready:\n"
                f"{_read_log(log_path)}"
            )
        try:
            with urllib.request.urlopen(url, timeout=1) as response:
                if 200 <= response.status < 300:
                    return
        except (OSError, urllib.error.URLError) as error:
            last_error = error
        time.sleep(0.25)

    raise RuntimeError(
        f"Timed out waiting for {url}: {last_error}\n{_read_log(log_path)}"
    )


def _wait_for_cdp_endpoint(port: int, process: subprocess.Popen, log_path: Path):
    url = f"http://127.0.0.1:{port}/json/version"
    deadline = time.time() + CDP_READY_TIMEOUT_SECONDS
    last_error = None

    while time.time() < deadline:
        if process.poll() is not None:
            raise RuntimeError(
                "Browser exited before CDP became ready:\n"
                f"{_read_log(log_path)}"
            )
        try:
            with urllib.request.urlopen(url, timeout=1) as response:
                data = json.load(response)
            if data.get("webSocketDebuggerUrl"):
                return
        except (OSError, ValueError, urllib.error.URLError) as error:
            last_error = error
        time.sleep(0.25)

    raise RuntimeError(
        "Timed out waiting for the browser CDP endpoint at "
        f"{url}: {last_error}\n{_read_log(log_path)}"
    )


async def _wait_for_context(browser):
    deadline = time.time() + CDP_READY_TIMEOUT_SECONDS
    while time.time() < deadline:
        if browser.contexts:
            return browser.contexts[0]
        await asyncio.sleep(0.1)
    raise RuntimeError("Browser connected over CDP but exposed no contexts")


async def _wait_for_page(context, *, url_prefix=None):
    deadline = time.time() + CDP_READY_TIMEOUT_SECONDS
    while time.time() < deadline:
        for page in context.pages:
            if url_prefix is None or page.url.startswith(url_prefix):
                return page
        await asyncio.sleep(0.1)
    if url_prefix is None:
        raise RuntimeError("Browser context became available but exposed no pages")
    raise RuntimeError(
        "Browser context became available but no page matched "
        f"{url_prefix!r}: {[page.url for page in context.pages]}"
    )


def _stop_process(process: subprocess.Popen):
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def _resolve_backend():
    base_url = os.environ.get("CLAWBROWSER_API_BASE_URL")
    api_key = os.environ.get("CLAWBROWSER_API_KEY")
    if base_url:
        return {
            "use_mock": False,
            "base_url": base_url,
            "api_key": api_key,
        }
    return {
        "use_mock": True,
        "base_url": None,
        "api_key": TEST_API_KEY,
    }


@asynccontextmanager
async def _launch_browser(
    *,
    fixture_name=None,
    skip_verify=False,
    expect_verify=False,
    verify_automation=False,
    fingerprints_fixture_path=DEFAULT_FINGERPRINTS_FIXTURE_PATH,
    proxy_fixture_path=DEFAULT_PROXY_FIXTURE_PATH,
):
    if expect_verify and skip_verify:
        raise ValueError("expect_verify and skip_verify are mutually exclusive")

    binary = _resolve_browser_binary()
    browser_port = _reserve_port()
    mock_port = _reserve_port()
    backend = _resolve_backend()

    with tempfile.TemporaryDirectory(prefix="clawbrowser-it-home-") as temp_home:
        home_dir = Path(temp_home)
        config_dir = _config_dir(home_dir)

        fingerprint_data = None
        if fixture_name is not None:
            fingerprint_data = _seed_profile(config_dir, fixture_name)

        mock_log_path = home_dir / "mock_server.log"
        browser_log_path = home_dir / "browser.log"
        mock_process = None
        config_base_url = backend["base_url"]
        if backend["use_mock"]:
            config_base_url = f"http://127.0.0.1:{mock_port}"

        _seed_config(config_dir, config_base_url, backend["api_key"])

        with mock_log_path.open("w", encoding="utf-8") as mock_log_file:
            mock_args = [
                sys.executable,
                str(MOCK_SERVER_SCRIPT),
                "--port",
                str(mock_port),
                "--fingerprints-fixture",
                str(fingerprints_fixture_path),
                "--proxy-fixture",
                str(proxy_fixture_path),
            ]
            mock_process = subprocess.Popen(
                mock_args,
                cwd=str(WORKSPACE_ROOT),
                stdout=mock_log_file,
                stderr=subprocess.STDOUT,
                text=True,
            )
            _wait_for_http_ready(
                f"http://127.0.0.1:{mock_port}/__healthz",
                mock_process,
                mock_log_path,
            )

            try:
                with browser_log_path.open("w", encoding="utf-8") as browser_log_file:
                    args = [
                        binary,
                        f"--remote-debugging-port={browser_port}",
                        *DEFAULT_BROWSER_ARGS,
                    ]
                    if fixture_name is not None:
                        args.append(f"--fingerprint={FINGERPRINT_ID}")
                        if skip_verify:
                            args.append("--skip-verify")
                        if verify_automation:
                            args.append("--verify-automation")
                    if not expect_verify:
                        args.append(f"http://127.0.0.1:{mock_port}/__blank")

                    browser_env = {
                        **os.environ,
                        "HOME": str(home_dir),
                        "CLAWBROWSER_CONFIG_DIR": str(config_dir),
                        "CLAWBROWSER_API_BASE_URL": config_base_url,
                    }
                    if backend["api_key"]:
                        browser_env["CLAWBROWSER_API_KEY"] = backend["api_key"]

                    browser_process = subprocess.Popen(
                        args,
                        env=browser_env,
                        stdout=browser_log_file,
                        stderr=subprocess.STDOUT,
                        text=True,
                    )

                    try:
                        _wait_for_cdp_endpoint(
                            browser_port, browser_process, browser_log_path
                        )
                        async with async_playwright() as playwright:
                            browser = await playwright.chromium.connect_over_cdp(
                                f"http://127.0.0.1:{browser_port}"
                            )
                            try:
                                context = await _wait_for_context(browser)
                                page = await _wait_for_page(
                                    context,
                                    url_prefix=(
                                        VERIFY_PAGE_URL if expect_verify else None
                                    ),
                                )
                                yield page, fingerprint_data
                            finally:
                                await browser.close()
                    finally:
                        if browser_process.poll() is not None:
                            _maybe_dump_log("browser log", browser_log_path)
                        _stop_process(browser_process)
            finally:
                if mock_process.poll() is not None:
                    _maybe_dump_log("mock server log", mock_log_path)
                _stop_process(mock_process)


@pytest_asyncio.fixture
async def browser_with_fingerprint():
    """Launch clawbrowser through --fingerprint and return a fresh test page."""
    async with _launch_browser(
        fixture_name="valid_fingerprint.json",
        skip_verify=True,
    ) as result:
        yield result


@pytest_asyncio.fixture
async def verify_browser_with_fingerprint():
    """Launch clawbrowser through --fingerprint and wait for clawbrowser://verify."""
    async with _launch_browser(
        fixture_name="valid_fingerprint.json",
        expect_verify=True,
        verify_automation=True,
    ) as result:
        yield result


@pytest_asyncio.fixture
async def verify_browser_with_proxy_mismatch():
    """Launch clawbrowser verify flow against a mismatched mock proxy response."""
    async with _launch_browser(
        fixture_name="valid_fingerprint.json",
        expect_verify=True,
        verify_automation=True,
        proxy_fixture_path=MISMATCH_PROXY_FIXTURE_PATH,
    ) as result:
        yield result


@pytest_asyncio.fixture
async def verify_browser_with_minimal_fingerprint():
    """Launch verify flow for a partial fingerprint without optional lists."""
    async with _launch_browser(
        fixture_name="minimal_fingerprint.json",
        expect_verify=True,
        verify_automation=True,
    ) as result:
        yield result


@pytest_asyncio.fixture
async def browser_with_absurd_fingerprint():
    """Launch clawbrowser with an obviously fake manual smoke fixture."""
    async with _launch_browser(
        fixture_name="absurd_fingerprint.json",
        skip_verify=True,
        fingerprints_fixture_path=PROJECT_ROOT / "api/mocks/fingerprints_absurd.json",
    ) as result:
        yield result


@pytest_asyncio.fixture
async def vanilla_browser():
    """Launch clawbrowser in vanilla mode (no fingerprint)."""
    async with _launch_browser() as result:
        page, _ = result
        yield page
