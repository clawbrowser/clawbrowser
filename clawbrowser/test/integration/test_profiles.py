"""Integration tests for multi-profile isolation and implicit startup."""

import os
import subprocess
import tempfile
from pathlib import Path

import pytest
from playwright.async_api import async_playwright
from conftest import (
    DEFAULT_BROWSER_ARGS,
    _config_dir,
    _launch_browser_with_details,
    _resolve_browser_binary,
    _reserve_port,
    _seed_config,
    _seed_profile,
    _wait_for_cdp_endpoint,
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

            assert ua2 == fp2["user_agent"]
            assert ua1 != ua2
            assert first_launch["config_dir"] != second_launch["config_dir"]
            assert first_launch["home_dir"] != second_launch["home_dir"]


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
                    assert ua == new_profile["response"]["fingerprint"]["user_agent"]
                    assert ua != old_profile["response"]["fingerprint"]["user_agent"]
                    await browser.close()
            finally:
                process.terminate()
                process.wait()
