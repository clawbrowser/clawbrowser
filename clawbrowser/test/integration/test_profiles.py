"""Integration tests for multi-profile isolation and implicit startup."""

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

import pytest
from playwright.async_api import async_playwright
from conftest import (
    DEFAULT_BROWSER_ARGS,
    FIXTURE_DIR,
    MOCK_SERVER_SCRIPT,
    DEFAULT_FINGERPRINTS_FIXTURE_PATH,
    DEFAULT_PROXY_FIXTURE_PATH,
    WORKSPACE_ROOT,
    _config_dir,
    _launch_browser_with_details,
    _resolve_browser_binary,
    _reserve_port,
    _seed_config,
    _seed_profile,
    _stop_process,
    _wait_for_cdp_endpoint,
    _wait_for_context,
    _wait_for_http_ready,
    _wait_for_page,
    TEST_API_KEY,
)


@pytest.mark.asyncio
async def test_multi_profile_isolation():
    """Verify concurrent launches from separate config dirs stay isolated."""
    async with _launch_browser_with_details(
        fixture_name="valid_fingerprint.json",
        skip_verify=True,
    ) as first_launch:
        page1 = first_launch["page"]
        fp1 = first_launch["fingerprint_data"]["response"]["fingerprint"]
        ua1 = await page1.evaluate("navigator.userAgent")
        assert ua1 == fp1["user_agent"]

        async with _launch_browser_with_details(
            fixture_name="absurd_fingerprint.json",
            skip_verify=True,
        ) as second_launch:
            page2 = second_launch["page"]
            fp2 = second_launch["fingerprint_data"]["response"]["fingerprint"]
            ua2 = await page2.evaluate("navigator.userAgent")

            # Legacy on-disk UA stays untouched; the runtime projects the
            # modern reduced token, without choosing another profile.
            assert ua2 == fp2["user_agent"].replace("Chrome/333.7.6.5", "Chrome/333.0.0.0")
            assert ua1 != ua2
            assert await page1.evaluate("navigator.userAgent") == ua1
            assert first_launch["config_dir"] != second_launch["config_dir"]
            assert first_launch["home_dir"] != second_launch["home_dir"]


def _normalized(path) -> str:
    return os.path.normcase(os.path.normpath(str(path)))


@pytest.mark.asyncio
async def test_same_config_fingerprints_run_isolated(tmp_path):
    """Two fingerprints from ONE config dir, launched directly at the same time.

    On Windows chrome_elf resolved User Data from the original command line, so
    every direct --fingerprint launch shared one directory and the second launch
    was handed to the first process. Each profile must get its own browser
    process, its own computed user data dir and its own cookies/storage.
    """
    binary = _resolve_browser_binary()
    mock_port = _reserve_port()
    base_url = f"http://127.0.0.1:{mock_port}"
    home_dir = tmp_path / "home"
    config_dir = _config_dir(home_dir)
    fixture = json.loads((FIXTURE_DIR / "valid_fingerprint.json").read_text())
    proxy = {**fixture["response"]["proxy"],
             "scheme": "http", "host": "127.0.0.1", "port": mock_port}
    profiles = ("isolation_a", "isolation_b")
    for fingerprint_id in profiles:
        _seed_profile(config_dir, "valid_fingerprint.json",
                      fingerprint_id=fingerprint_id, proxy_config=proxy)
    _seed_config(config_dir, base_url, TEST_API_KEY)
    # The user data dir bug only exists for a real window: headless Chromium
    # already gets a unique temporary directory from chrome_elf.
    headless = (sys.platform != "win32"
                and os.environ.get("CLAWBROWSER_TEST_HEADFUL") != "1")
    browser_args = [arg for arg in DEFAULT_BROWSER_ARGS
                    if headless or not arg.startswith("--headless")]
    env = {**os.environ, "HOME": str(home_dir),
           "CLAWBROWSER_CONFIG_DIR": str(config_dir),
           "CLAWBROWSER_API_BASE_URL": base_url,
           "CLAWBROWSER_API_KEY": TEST_API_KEY}

    mock_log = tmp_path / "mock.log"
    processes = []
    with mock_log.open("w", encoding="utf-8") as mock_log_file:
        mock = subprocess.Popen(
            [sys.executable, str(MOCK_SERVER_SCRIPT), "--port", str(mock_port),
             "--fingerprints-fixture", str(DEFAULT_FINGERPRINTS_FIXTURE_PATH),
             "--proxy-fixture", str(DEFAULT_PROXY_FIXTURE_PATH)],
            cwd=str(WORKSPACE_ROOT), stdout=mock_log_file,
            stderr=subprocess.STDOUT, text=True)
        try:
            _wait_for_http_ready(f"{base_url}/__healthz", mock, mock_log)
            launches = []
            for fingerprint_id in profiles:
                port = _reserve_port()
                log_path = tmp_path / f"{fingerprint_id}.log"
                with log_path.open("w", encoding="utf-8") as log_file:
                    process = subprocess.Popen(
                        [binary, f"--remote-debugging-port={port}", *browser_args,
                         f"--fingerprint={fingerprint_id}", "--skip-verify",
                         f"{base_url}/__blank"],
                        env=env, stdout=log_file, stderr=subprocess.STDOUT,
                        text=True)
                processes.append(process)
                # A launch handed to an existing browser exits here instead of
                # opening its own DevTools endpoint.
                _wait_for_cdp_endpoint(port, process, log_path)
                launches.append((fingerprint_id, process, port))

            async with async_playwright() as playwright:
                pages = {}
                for fingerprint_id, process, port in launches:
                    browser = await playwright.chromium.connect_over_cdp(
                        f"http://127.0.0.1:{port}")
                    context = await _wait_for_context(browser)
                    pages[fingerprint_id] = await _wait_for_page(context)

                user_data_dirs = {}
                for fingerprint_id, page in pages.items():
                    await page.goto("chrome://version")
                    profile_path = await page.locator("#profile_path").inner_text()
                    expected = config_dir / "Browser" / fingerprint_id
                    assert _normalized(profile_path).startswith(
                        _normalized(expected)), (fingerprint_id, profile_path)
                    user_data_dirs[fingerprint_id] = _normalized(profile_path)
                assert len(set(user_data_dirs.values())) == 2, user_data_dirs

                first, second = (pages[p] for p in profiles)
                for page in (first, second):
                    await page.goto(f"{base_url}/__blank")
                await first.evaluate("""() => {
                    document.cookie = 'isolation=a; max-age=3600; path=/';
                    localStorage.setItem('isolation', 'a');
                }""")
                await second.reload()
                assert await second.evaluate("document.cookie") == ""
                assert await second.evaluate(
                    "localStorage.getItem('isolation')") is None
                await second.evaluate(
                    "localStorage.setItem('isolation', 'b')")
                await first.reload()
                assert await first.evaluate("document.cookie") == "isolation=a"
                assert await first.evaluate(
                    "localStorage.getItem('isolation')") == "a"

            pids = [process.pid for _, process, _ in launches]
            assert len(set(pids)) == 2, pids
            assert all(process.poll() is None for _, process, _ in launches)
        finally:
            for process in processes:
                _stop_process(process)
            _stop_process(mock)


@pytest.mark.asyncio
async def test_implicit_startup_newest_profile():
    """No-arg launch should pick the newest cached profile."""
    binary = _resolve_browser_binary()
    browser_port = _reserve_port()

    with tempfile.TemporaryDirectory(prefix="clawbrowser-implicit-", ignore_cleanup_errors=True) as temp_home:
        home_dir = Path(temp_home)
        config_dir = _config_dir(home_dir)
        _seed_config(config_dir, "http://127.0.0.1:0", TEST_API_KEY)

        old_profile = _seed_profile(
            config_dir,
            "valid_fingerprint.json",
            fingerprint_id="profile_old",
            created_at="2026-04-16T10:00:00Z",
        )
        new_profile = _seed_profile(
            config_dir,
            "absurd_fingerprint.json",
            fingerprint_id="profile_new",
            created_at="2026-04-16T10:05:00Z",
        )

        args = [
            binary,
            f"--remote-debugging-port={browser_port}",
            "--skip-verify",
            *DEFAULT_BROWSER_ARGS,
        ]
        browser_env = {**os.environ, "HOME": str(home_dir), "CLAWBROWSER_CONFIG_DIR": str(config_dir)}

        browser_log_path = home_dir / "browser.log"
        with browser_log_path.open("w", encoding="utf-8") as log_file:
            process = subprocess.Popen(args, env=browser_env, stdout=log_file, stderr=subprocess.STDOUT)
            try:
                _wait_for_cdp_endpoint(browser_port, process, browser_log_path)
                async with async_playwright() as playwright:
                    browser = await playwright.chromium.connect_over_cdp(f"http://127.0.0.1:{browser_port}")
                    page = browser.contexts[0].pages[0]
                    ua = await page.evaluate("navigator.userAgent")
                    assert ua == new_profile["response"]["fingerprint"]["user_agent"].replace(
                        "Chrome/333.7.6.5", "Chrome/333.0.0.0")
                    assert ua != old_profile["response"]["fingerprint"]["user_agent"]
                    await browser.close()
            finally:
                process.terminate()
                process.wait()
