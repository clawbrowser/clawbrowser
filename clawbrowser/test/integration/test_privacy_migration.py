"""Legacy privacy policy regeneration; controlled backend, not live API E2E."""

import copy
import json
import re
from pathlib import Path

import pytest

from conftest import _launch_browser_with_details, _read_saved_profile


@pytest.mark.asyncio
async def test_cached_full_ua_is_reduced_without_regeneration(tmp_path):
    fixture = Path(__file__).resolve().parents[1] / "fixtures/valid_fingerprint.json"
    cached = json.loads(fixture.read_text(encoding="utf-8"))
    fp = cached["response"]["fingerprint"]
    fp["browser_version"] = "151.0.7922.71"
    fp["user_agent"] = re.sub(r"Chrome/[\d.]+", "Chrome/151.0.7922.71", fp["user_agent"])
    fp["headers"]["User-Agent"] = fp["user_agent"]
    fp["user_agent_data"]["uaFullVersion"] = fp["browser_version"]
    for brand in fp["user_agent_data"]["fullVersionList"]:
        if brand["brand"] in ("Chromium", "Google Chrome"):
            brand["version"] = fp["browser_version"]
    for brand in fp["user_agent_data"]["brands"]:
        if brand["brand"] in ("Chromium", "Google Chrome"):
            brand["version"] = "151"
    old_path = tmp_path / "legacy-ua.json"
    old_path.write_text(json.dumps(cached), encoding="utf-8")
    expected = fp["user_agent"].replace("151.0.7922.71", "151.0.0.0")
    async with _launch_browser_with_details(
        fixture_name=str(old_path), fingerprint_id="qa_cached_ua",
        backend_mode="mock", skip_verify=True,
    ) as launch:
        page = launch["page"]
        await page.goto("data:text/html,<title>Cached UA regression</title>")
        assert await page.evaluate("navigator.userAgent") == expected
        worker_ua = await page.evaluate("""async () => {
            const url = URL.createObjectURL(new Blob([
                'postMessage(navigator.userAgent)'], {type:'text/javascript'}));
            const worker = new Worker(url);
            let timer;
            try {
                return await new Promise((resolve, reject) => {
                    timer = setTimeout(() => reject(Error('worker timeout')), 5000);
                    worker.onmessage = e => resolve(e.data);
                    worker.onerror = () => reject(Error('worker failed'));
                });
            } finally {clearTimeout(timer);worker.terminate();URL.revokeObjectURL(url);}
        }""")
        assert worker_ua == expected
        saved = _read_saved_profile(launch["config_dir"], "qa_cached_ua")
        assert saved is not None
        for key in ("user_agent", "browser_version", "canvas_seed", "audio_seed",
                    "client_rects_seed", "timezone"):
            assert saved["response"]["fingerprint"][key] == fp[key], key
        log = (launch["home_dir"] / "mock_server.log").read_text(encoding="utf-8")
        assert "POST /v1/fingerprints/generate" not in log


@pytest.mark.asyncio
@pytest.mark.parametrize("legacy", ["native_canvas", "native_fonts", "missing_gpu"])
@pytest.mark.parametrize("backend_accepts", [True, False])
async def test_legacy_privacy_policy_regenerates(legacy, backend_accepts, tmp_path):
    fixture = Path(__file__).resolve().parents[1] / "fixtures/valid_fingerprint.json"
    clean = json.loads(fixture.read_text(encoding="utf-8"))
    cached = copy.deepcopy(clean)
    if legacy == "native_canvas":
        cached["response"]["fingerprint"]["surface_policy"]["canvas"]["mode"] = "native"
    elif legacy == "native_fonts":
        cached["response"]["fingerprint"]["surface_policy"]["fonts"]["mode"] = "native"
    else:
        cached["request"].pop("runtime_gpu")
    old_path = tmp_path / "legacy.json"
    old_path.write_text(json.dumps(cached), encoding="utf-8")
    # Cached targeting is preserved even when it differs from the host platform.
    expected = {k: v for k, v in clean["request"].items() if not k.startswith("runtime_")}
    if not backend_accepts:
        expected["country"] = "INTENTIONAL_REJECTION"
    response = copy.deepcopy(clean["response"]["fingerprint"])
    response["canvas_seed"] = 314159265
    mock_path = tmp_path / "regenerated.json"
    mock_path.write_text(json.dumps({
        "request": expected, "response": {"fingerprint": response},
    }), encoding="utf-8")
    context = _launch_browser_with_details(
        fixture_name=str(old_path), fingerprint_id="qa_legacy_privacy",
        backend_mode="mock", skip_verify=True,
        fingerprints_fixture_path=mock_path,
    )
    if not backend_accepts:
        with pytest.raises(RuntimeError, match="Browser exited before CDP became ready") as failure:
            async with context:
                pytest.fail("Browser exposed CDP despite failed privacy regeneration")
        # Reject unrelated startup crashes as evidence of fail-closed behavior.
        assert "request body did not match mock fixture" in str(failure.value)
        return
    async with context as launch:
        saved = _read_saved_profile(launch["config_dir"], "qa_legacy_privacy")
        assert saved is not None
        fp = saved["response"]["fingerprint"]
        assert fp["canvas_seed"] == 314159265, "cached identity was not regenerated"
        assert fp["surface_policy"]["canvas"]["mode"] == "override"
        assert fp["surface_policy"]["fonts"]["mode"] == "native_or_allowlist"
        assert saved["request"]["runtime_gpu"].startswith("swiftshader")
        browser = launch["page"].context.browser
        assert saved["request"]["runtime_browser_version"] == browser.version
        assert await launch["page"].evaluate("navigator.userAgent") == fp["user_agent"]
        log = (launch["home_dir"] / "mock_server.log").read_text(encoding="utf-8")
        assert 'POST /v1/fingerprints/generate HTTP/1.1" 200' in log
