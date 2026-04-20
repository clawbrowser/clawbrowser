"""Integration tests for CDP and runtime stability."""

import json
import urllib.request

import pytest

from conftest import _launch_browser, _launch_browser_with_details


@pytest.mark.asyncio
async def test_cdp_json_version():
    """Verify CDP /json/version endpoint returns expected metadata."""
    async with _launch_browser_with_details(
        fixture_name="valid_fingerprint.json",
        skip_verify=True,
    ) as launch:
        with urllib.request.urlopen(
            f"http://127.0.0.1:{launch['browser_port']}/json/version",
            timeout=5,
        ) as response:
            payload = json.load(response)

        assert payload["Browser"]
        assert payload["Protocol-Version"]
        assert payload["webSocketDebuggerUrl"].startswith("ws://127.0.0.1:")


@pytest.mark.asyncio
async def test_fingerprint_stability_across_tabs():
    """Verify that fingerprinted values remain identical across multiple tabs."""
    async with _launch_browser(
        fixture_name="valid_fingerprint.json",
        skip_verify=True,
    ) as (page1, fp):
        fingerprint = fp["response"]["fingerprint"]
        context = page1.context
        page2 = await context.new_page()

        ua1 = await page1.evaluate("navigator.userAgent")
        ua2 = await page2.evaluate("navigator.userAgent")

        assert ua1 == ua2
        assert ua1 == fingerprint["user_agent"]

        # Check other properties like screen
        screen1 = await page1.evaluate("screen.width")
        screen2 = await page2.evaluate("screen.width")
        assert screen1 == screen2
        assert screen1 == fingerprint["screen"]["width"]


@pytest.mark.asyncio
async def test_fingerprint_stability_second_renderer():
    """Verify stability even when a second renderer process might be used."""
    async with _launch_browser(
        fixture_name="valid_fingerprint.json",
        skip_verify=True,
    ) as (page1, fp):
        fingerprint = fp["response"]["fingerprint"]
        page2 = await page1.context.new_page()
        await page2.goto("data:text/html,<html><body>Second renderer</body></html>")

        ua1 = await page1.evaluate("navigator.userAgent")
        ua2 = await page2.evaluate("navigator.userAgent")

        assert ua1 == ua2
        assert ua1 == fingerprint["user_agent"]
