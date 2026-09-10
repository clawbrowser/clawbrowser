"""Integration tests: managed fingerprint proxies and WebRTC leak prevention."""

import json
import os

import pytest

from clawbrowser.test.integration.test_surfaces import _timezones_match
from clawbrowser.test.integration.webrtc_probe import (
    assert_relay_only,
    collect_webrtc_observations,
)


async def open_popup(page, url: str):
    existing_pages = list(page.context.pages)
    await page.evaluate("targetUrl => window.open(targetUrl, '_blank')", url)
    for _ in range(50):
        for candidate in page.context.pages:
            if candidate not in existing_pages:
                return candidate
        await page.wait_for_timeout(100)
    raise AssertionError(f"Popup did not open for {url}")


def expected_verify_surfaces(data):
    surfaces = []
    fingerprint = data["response"]["fingerprint"]
    if "media_devices" in fingerprint:
        surfaces.append("mediaDevices")
    if "plugins" in fingerprint:
        surfaces.append("plugins")
    if "proxy" in data["response"]:
        surfaces.append("proxy")
        surfaces.append("webrtc.iceCandidates")
    if "speech_voices" in fingerprint:
        surfaces.append("speechSynthesis.voices")
    return surfaces


def check_for_surface(result, surface):
    for check in result["checks"]:
        if check["surface"] == surface:
            return check
    raise AssertionError(f"Missing {surface!r} check in {result['checks']}")


def json_surface(result, surface):
    check = check_for_surface(result, surface)
    return json.loads(check["actual"]), json.loads(check["expected"])


@pytest.mark.asyncio
async def test_fingerprint_proxy_webrtc_no_direct_candidates(
    browser_with_fingerprint,
):
    """A fingerprint-backed proxy must not expose direct WebRTC routes."""
    page, data = browser_with_fingerprint
    if "proxy" not in data["response"]:
        pytest.skip("No proxy in test fixture")

    observations = await collect_webrtc_observations(
        page,
        [[]],
    )
    assert_relay_only(observations)


@pytest.mark.asyncio
async def test_webrtc_controlled_stun_no_direct_candidates(
    browser_with_fingerprint,
):
    """An opt-in controlled STUN probe must expose no direct route."""
    stun_url = os.environ.get("CLAWBROWSER_REAL_STUN_URL")
    if not stun_url:
        pytest.skip("CLAWBROWSER_REAL_STUN_URL is not configured")

    page, data = browser_with_fingerprint
    if "proxy" not in data["response"]:
        pytest.skip("No proxy in test fixture")

    observations = await collect_webrtc_observations(
        page,
        [[{"urls": stun_url}]],
    )
    assert_relay_only(observations)


@pytest.mark.asyncio
async def test_verify_page(verify_browser_with_fingerprint):
    """Verify page should report all checks passing."""
    page, data = verify_browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    required_surfaces = expected_verify_surfaces(data)
    await page.wait_for_function(
        """requiredSurfaces => {
        const result = window.__clawbrowser_verify;
        if (!result) {
            return false;
        }
        if (result.status !== 'pass' && result.status !== 'fail') {
            return false;
        }
        const surfaces = new Set((result.checks || []).map(
            check => check.surface
        ));
        return requiredSurfaces.every(surface => surfaces.has(surface));
    }""",
        arg=required_surfaces,
    )
    result = await page.evaluate("window.__clawbrowser_verify")
    actual, expected = json_surface(result, "mediaDevices")
    assert actual == expected == fp["media_devices"]
    actual, expected = json_surface(result, "plugins")
    assert actual == expected == fp["plugins"]
    if "proxy" in data["response"]:
        proxy_check = check_for_surface(result, "proxy")
        assert proxy_check["pass"] is True
        assert proxy_check["actual_country"] == "US"
        assert proxy_check["actual_city"] == "New York"
        webrtc_check = check_for_surface(result, "webrtc.iceCandidates")
        assert webrtc_check["pass"] is True
    if "speech_voices" in fp:
        actual, expected = json_surface(result, "speechSynthesis.voices")
        assert actual == expected == fp["speech_voices"]
    assert result["status"] == "pass", f"Failed checks: {result['checks']}"


@pytest.mark.asyncio
async def test_verify_page_proxy_mismatch(verify_browser_with_proxy_mismatch):
    """Verify page should fail when proxy verification returns a mismatch."""
    page, data = verify_browser_with_proxy_mismatch
    fp = data["response"]["fingerprint"]
    required_surfaces = expected_verify_surfaces(data)
    await page.wait_for_function(
        """requiredSurfaces => {
        const result = window.__clawbrowser_verify;
        if (!result) {
            return false;
        }
        if (result.status !== 'pass' && result.status !== 'fail') {
            return false;
        }
        const surfaces = new Set((result.checks || []).map(
            check => check.surface
        ));
        return requiredSurfaces.every(surface => surfaces.has(surface));
    }""",
        arg=required_surfaces,
    )
    result = await page.evaluate("window.__clawbrowser_verify")
    actual, expected = json_surface(result, "mediaDevices")
    assert actual == expected == fp["media_devices"]
    actual, expected = json_surface(result, "plugins")
    assert actual == expected == fp["plugins"]
    proxy_check = check_for_surface(result, "proxy")
    assert proxy_check["pass"] is False
    assert proxy_check["actual_country"] == "CA"
    assert proxy_check["actual_city"] == "Toronto"
    webrtc_check = check_for_surface(result, "webrtc.iceCandidates")
    assert webrtc_check["pass"] is True
    if "speech_voices" in fp:
        actual, expected = json_surface(result, "speechSynthesis.voices")
        assert actual == expected == fp["speech_voices"]
    assert result["status"] == "fail", result["checks"]


@pytest.mark.asyncio
async def test_verify_page_expected_payload(verify_browser_with_fingerprint):
    """Verify page should expose exact JSON payloads for list surfaces."""
    page, data = verify_browser_with_fingerprint
    fp = data["response"]["fingerprint"]

    await page.wait_for_function("() => !!window.__clawbrowser_expected")
    expected = await page.evaluate("window.__clawbrowser_expected")

    assert "media_devices_json" in expected
    assert "plugins_json" in expected
    assert "speech_voices_json" in expected
    assert json.loads(expected["media_devices_json"]) == fp["media_devices"]
    assert json.loads(expected["plugins_json"]) == fp["plugins"]
    assert json.loads(expected["speech_voices_json"]) == fp["speech_voices"]


@pytest.mark.asyncio
async def test_verify_page_json_helpers_normalize_key_order(
    verify_browser_with_fingerprint,
):
    """Verify-page JSON helpers should compare semantic equality, not key order."""
    page, _ = verify_browser_with_fingerprint

    await page.wait_for_function("() => !!window.__clawbrowser_verify_helpers")
    result = await page.evaluate("""() => {
        const helpers = window.__clawbrowser_verify_helpers;
        return {
            media_devices: helpers.sameJson(
                JSON.stringify([
                    {kind: 'audioinput', label: 'Mic', device_id: 'device-1'}
                ]),
                [
                    {device_id: 'device-1', label: 'Mic', kind: 'audioinput'}
                ]
            ),
            plugins: helpers.sameJson(
                JSON.stringify([
                    {name: 'PDF Viewer', description: 'Portable Document Format', filename: 'internal-pdf'}
                ]),
                [
                    {filename: 'internal-pdf', description: 'Portable Document Format', name: 'PDF Viewer'}
                ]
            ),
        };
    }""")

    assert result == {"media_devices": True, "plugins": True}


@pytest.mark.asyncio
async def test_verify_page_skips_absent_optional_lists(
    verify_browser_with_minimal_fingerprint,
):
    """Absent optional list surfaces should not be compared against empty lists."""
    page, _ = verify_browser_with_minimal_fingerprint

    await page.wait_for_function(
        """() => {
        const result = window.__clawbrowser_verify;
        return result && (result.status === 'pass' || result.status === 'fail');
    }"""
    )

    expected = await page.evaluate("window.__clawbrowser_expected")
    assert expected["media_devices_json"] == ""
    assert expected["plugins_json"] == ""
    assert expected["speech_voices_json"] == ""

    result = await page.evaluate("window.__clawbrowser_verify")
    surfaces = {check["surface"] for check in result["checks"]}
    assert "mediaDevices" not in surfaces
    assert "plugins" not in surfaces
    assert "speechSynthesis.voices" not in surfaces
    assert result["status"] == "pass", result["checks"]


@pytest.mark.asyncio
async def test_cross_tab_consistency(browser_with_fingerprint):
    """Two tabs should show the same fingerprint values."""
    page1, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]
    page2 = await open_popup(page1, page1.url)

    ua1 = await page1.evaluate("navigator.userAgent")
    ua2 = await page2.evaluate("navigator.userAgent")
    assert ua1 == ua2 == fp["user_agent"]

    await page2.close()


@pytest.mark.asyncio
async def test_cross_process_consistency(browser_with_fingerprint):
    """A page that forces a new renderer process should have the same fingerprint."""
    page1, data = browser_with_fingerprint
    fp = data["response"]["fingerprint"]

    page2 = await open_popup(page1, page1.url)
    await page2.goto("data:text/html,<h1>cross-process</h1>")

    ua1 = await page1.evaluate("navigator.userAgent")
    ua2 = await page2.evaluate("navigator.userAgent")
    assert ua1 == ua2 == fp["user_agent"]

    hw1 = await page1.evaluate("navigator.hardwareConcurrency")
    hw2 = await page2.evaluate("navigator.hardwareConcurrency")
    assert hw1 == hw2 == fp["hardware"]["concurrency"]

    tz1 = await page1.evaluate(
        "Intl.DateTimeFormat().resolvedOptions().timeZone"
    )
    tz2 = await page2.evaluate(
        "Intl.DateTimeFormat().resolvedOptions().timeZone"
    )
    assert tz1 == tz2
    assert await _timezones_match(page1, fp["timezone"])

    await page2.close()
