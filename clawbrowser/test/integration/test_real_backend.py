"""Integration tests for real backend and proxy validation."""

import os

import pytest

from conftest import VERIFY_PAGE_URL, _launch_browser_with_details


def _find_check(result, surface):
    for check in result["checks"]:
        if check["surface"] == surface:
            return check
    raise AssertionError(f"Missing {surface!r} check in {result['checks']}")


@pytest.mark.asyncio
async def test_real_backend_fingerprint_fetch():
    """Verify that we can fetch a real fingerprint from the dev backend."""
    api_key = os.environ.get("CLAWBROWSER_API_KEY")
    api_base_url = os.environ.get("CLAWBROWSER_API_BASE_URL")

    if not api_key or not api_base_url:
        pytest.skip("CLAWBROWSER_API_KEY or CLAWBROWSER_API_BASE_URL not set")

    fingerprint_id = f"test_real_{os.getpid()}"

    async with _launch_browser_with_details(
        fixture_name=None,
        fingerprint_id=fingerprint_id,
        backend_mode="real",
        skip_verify=True,
    ) as launch:
        page = launch["page"]
        saved_profile = launch["fingerprint_data"]
        assert saved_profile is not None
        assert saved_profile["profile_id"] == fingerprint_id
        assert saved_profile["request"]["browser"] == "chrome"

        fingerprint = saved_profile["response"]["fingerprint"]
        ua = await page.evaluate("navigator.userAgent")
        assert ua == fingerprint["user_agent"]


@pytest.mark.asyncio
async def test_real_proxy_verification():
    """Verify that proxy reachability works with real backend data via verify-page."""
    api_key = os.environ.get("CLAWBROWSER_API_KEY")
    api_base_url = os.environ.get("CLAWBROWSER_API_BASE_URL")

    if not api_key or not api_base_url:
        pytest.skip("CLAWBROWSER_API_KEY or CLAWBROWSER_API_BASE_URL not set")

    async with _launch_browser_with_details(
        fixture_name=None,
        fingerprint_id=f"test_verify_{os.getpid()}",
        backend_mode="real",
        expect_verify=True,
        verify_automation=True,
    ) as launch:
        page = launch["page"]
        assert page.url.startswith(VERIFY_PAGE_URL)

        await page.wait_for_function(
            """() => {
                const result = window.__clawbrowser_verify;
                return result && (result.status === 'pass' || result.status === 'fail');
            }""",
            timeout=60000,
        )
        result = await page.evaluate("window.__clawbrowser_verify")
        proxy_check = _find_check(result, "proxy")
        assert proxy_check["pass"] is True
