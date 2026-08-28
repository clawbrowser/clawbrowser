"""Opt-in integration test for authenticated SOCKS5 proxy routing."""

import json
import os

import pytest

from conftest import _launch_browser_with_details


def _extract_ip(response_text: str) -> str:
    text = response_text.strip()
    try:
        payload = json.loads(text)
    except json.JSONDecodeError:
        return text

    for key in ("ip", "origin", "query"):
        value = payload.get(key)
        if isinstance(value, str) and value.strip():
            return value.split(",", 1)[0].strip()
    raise AssertionError(f"Could not find an IP address in response: {payload}")


@pytest.mark.asyncio
async def test_real_socks5_auth_proxy_exits_expected_ip():
    proxy_url = os.environ.get("CLAWBROWSER_REAL_SOCKS5_PROXY_URL")
    expected_ip = os.environ.get("CLAWBROWSER_REAL_SOCKS5_EXPECTED_IP")
    if not proxy_url or not expected_ip:
        pytest.skip(
            "Set CLAWBROWSER_REAL_SOCKS5_PROXY_URL and "
            "CLAWBROWSER_REAL_SOCKS5_EXPECTED_IP to run"
        )

    echo_url = os.environ.get(
        "CLAWBROWSER_REAL_SOCKS5_ECHO_URL",
        "https://api.ipify.org?format=json",
    )

    async with _launch_browser_with_details(
        fixture_name="valid_fingerprint.json",
        backend_mode="mock",
        skip_verify=True,
        extra_env={"CLAWBROWSER_DEV_PROXY_URL": proxy_url},
        headless=False,
    ) as launch:
        page = launch["page"]
        await page.goto(echo_url, wait_until="networkidle")
        actual_ip = _extract_ip(await page.locator("body").inner_text())

    assert actual_ip == expected_ip
