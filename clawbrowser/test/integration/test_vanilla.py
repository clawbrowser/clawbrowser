"""Integration tests: verify vanilla mode (no fingerprint spoofing)."""

import pytest

DEFAULT_FIXTURE_USER_AGENT = (
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
    "AppleWebKit/537.36 (KHTML, like Gecko) "
    "Clawbrowser/120.0.0.0 Safari/537.36"
)
ABSURD_FIXTURE_USER_AGENT = (
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
    "AppleWebKit/537.36 (KHTML, like Gecko) "
    "Clawbrowser/333.7.6.5 Safari/537.36"
)


@pytest.mark.asyncio
async def test_vanilla_no_spoofing(vanilla_browser):
    """In vanilla mode, navigator values should be real (not spoofed)."""
    page = vanilla_browser
    ua = await page.evaluate("navigator.userAgent")
    assert isinstance(ua, str)
    assert ua
    assert ua != DEFAULT_FIXTURE_USER_AGENT
    assert ua != ABSURD_FIXTURE_USER_AGENT
    # Screen should return real values (not exactly 1920x1080 from fixture)
    width = await page.evaluate("screen.width")
    assert isinstance(width, int)
    assert width > 0
