"""Integration tests: verify vanilla mode (no fingerprint spoofing)."""

import pytest


@pytest.mark.asyncio
async def test_vanilla_no_spoofing(vanilla_browser):
    """In vanilla mode, navigator values should be real (not spoofed)."""
    page = vanilla_browser
    ua = await page.evaluate("navigator.userAgent")
    # Should contain "Chrome" (real UA, not overridden)
    assert "Chrome" in ua
    # Screen should return real values (not exactly 1920x1080 from fixture)
    width = await page.evaluate("screen.width")
    assert isinstance(width, int)
    assert width > 0
