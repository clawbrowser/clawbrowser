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
async def test_webdriver_hidden_without_faking_scripted_events():
    async with _launch_browser(
        fixture_name="valid_fingerprint.json",
        skip_verify=True,
    ) as (page, _):
        result = await page.evaluate("""() => {
            const event = new MouseEvent('click');
            return {
                webdriver: navigator.webdriver,
                scriptedTrusted: event.isTrusted
            };
        }""")

        assert result["webdriver"] is False
        assert result["scriptedTrusted"] is False


@pytest.mark.asyncio
async def test_cdp_click_is_trusted_and_sets_user_activation():
    async with _launch_browser(
        fixture_name="valid_fingerprint.json",
        skip_verify=True,
    ) as (page, _):
        await page.set_content("""<!doctype html>
            <button id="target" style="position:absolute;left:20px;top:20px;width:120px;height:40px">Click</button>
            <script>
              window.__clawClick = null;
              document.getElementById('target').addEventListener('click', event => {
                window.__clawClick = {
                  trusted: event.isTrusted,
                  active: navigator.userActivation.isActive,
                  hasBeenActive: navigator.userActivation.hasBeenActive
                };
              });
            </script>
        """)

        box = await page.locator("#target").bounding_box()
        assert box is not None
        await page.mouse.click(box["x"] + box["width"] / 2, box["y"] + box["height"] / 2)
        result = await page.evaluate("window.__clawClick")

        assert result == {
            "trusted": True,
            "active": True,
            "hasBeenActive": True,
        }


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
