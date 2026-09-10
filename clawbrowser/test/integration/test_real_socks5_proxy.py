"""Opt-in integration test for authenticated SOCKS5 proxy routing."""

import json
import os

import pytest

from conftest import _launch_browser_with_details
from webrtc_probe import assert_relay_only, collect_webrtc_observations


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


@pytest.mark.asyncio
async def test_real_socks5_auth_proxy_webrtc_uses_tcp_turn_relay():
    """Headful release control: TURN must work without a direct ICE route."""
    proxy_url = os.environ.get("CLAWBROWSER_REAL_SOCKS5_PROXY_URL")
    expected_ip = os.environ.get("CLAWBROWSER_REAL_SOCKS5_EXPECTED_IP")
    turn_url = os.environ.get("CLAWBROWSER_REAL_TURN_URL")
    if not proxy_url or not expected_ip or not turn_url:
        pytest.skip(
            "Set the real SOCKS5 proxy, expected IP, and TCP/TLS TURN env vars"
        )

    normalized_turn_url = turn_url.lower()
    assert normalized_turn_url.startswith("turns:") or "transport=tcp" in (
        normalized_turn_url
    ), "CLAWBROWSER_REAL_TURN_URL must use TURN over TCP or TLS"

    turn_username = os.environ.get("CLAWBROWSER_REAL_TURN_USERNAME")
    turn_credential = os.environ.get("CLAWBROWSER_REAL_TURN_CREDENTIAL")
    assert bool(turn_username) == bool(turn_credential), (
        "Set both CLAWBROWSER_REAL_TURN_USERNAME and "
        "CLAWBROWSER_REAL_TURN_CREDENTIAL, or neither"
    )
    turn_server = {"urls": turn_url}
    if turn_username and turn_credential:
        turn_server.update(
            {"username": turn_username, "credential": turn_credential}
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

        observations = await collect_webrtc_observations(
            page,
            [[turn_server]],
        )
        assert_relay_only(observations, require_relay=True)
