"""Do not count an ignored experiment flag as network acceptance."""

import pytest

from canvas_network_mode import canvas_network_args, assert_canvas_network_mode


@pytest.mark.parametrize("mode,enabled", [("0", False), ("1", True)])
def test_network_canvas_args(monkeypatch, mode, enabled):
    monkeypatch.setenv("CLAWBROWSER_QA_NORMALIZED_CANVAS", mode)
    assert bool(canvas_network_args()) is enabled


def test_network_canvas_rejects_bad_mode(monkeypatch):
    monkeypatch.setenv("CLAWBROWSER_QA_NORMALIZED_CANVAS", "true")
    with pytest.raises(ValueError):
        canvas_network_args()


@pytest.mark.asyncio
@pytest.mark.parametrize("mode,changed,passes", [
    ("0", 368, True), ("1", 0, True), ("0", 0, False), ("1", 368, False)])
async def test_network_canvas_canary(monkeypatch, mode, changed, passes):
    monkeypatch.setenv("CLAWBROWSER_QA_NORMALIZED_CANVAS", mode)

    class Page:
        async def evaluate(self, _):
            return {"changed": changed, "maxDelta": 1 if changed else 0, "badAlpha": 0}

    if passes:
        result = await assert_canvas_network_mode(Page())
        assert result["mode"] == ("normalized" if mode == "1" else "protected")
    else:
        with pytest.raises(AssertionError):
            await assert_canvas_network_mode(Page())
