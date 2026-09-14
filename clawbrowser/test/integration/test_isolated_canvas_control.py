"""Diagnostic gate for native canvas on an isolated Linux renderer.

Passing this limited matrix alone does not authorize changing the default policy.
It is not a hardware/architecture independence proof.
"""
import hashlib
import json
import os
import sys

import pytest

from conftest import FIXTURE_DIR, _launch_browser_with_details


@pytest.mark.asyncio
async def test_isolated_native_canvas_across_font_and_raster_controls(tmp_path, record_property):
    configs = [os.environ.get("CLAWBROWSER_TEST_FONTCONFIG_" + x) for x in ("A", "B")]
    if sys.platform != "linux" or not all(configs):
        pytest.skip("requires two Linux host-font controls")
    fixture = json.loads((FIXTURE_DIR / "valid_fingerprint.json").read_text())
    fixture["response"]["fingerprint"]["surface_policy"]["canvas"]["mode"] = "native"
    path = tmp_path / "native-canvas-control.json"
    path.write_text(json.dumps(fixture))
    # Native cached canvas triggers privacy regeneration. Return the control
    # policy through the mock API too, matching preserved cached targeting.
    mock = tmp_path / "native-canvas-api.json"
    mock.write_text(json.dumps({
        "request": {k: v for k, v in fixture["request"].items()
                    if not k.startswith("runtime_")},
        "response": fixture["response"],
    }))
    results = []
    for config in configs:
        for args in ((), ("--disable-skia-runtime-opts",), ("--disable-accelerated-2d-canvas",)):
            async with _launch_browser_with_details(
                fixture_name=str(path), backend_mode="mock", skip_verify=True,
                headless=False, extra_env={"FONTCONFIG_FILE": config},
                extra_browser_args=args,
                fingerprints_fixture_path=mock,
            ) as launch:
                result = await launch["page"].evaluate("""() => {
                    const canvas = document.createElement('canvas');
                    canvas.width = 480; canvas.height = 240;
                    const c = canvas.getContext('2d');
                    const g = c.createLinearGradient(0,0,480,200);
                    g.addColorStop(0, '#123456'); g.addColorStop(1, '#fedcba');
                    c.fillStyle = g; c.fillRect(0,0,480,240);
                    c.fillStyle = '#1b273c';
                    ['sans-serif', 'serif', 'monospace', 'Arimo', 'DejaVu Sans'].forEach((f,i) => {
                        c.font = '19px ' + (['sans-serif','serif','monospace'].includes(f) ? f : JSON.stringify(f));
                        c.fillText('Latin 0123 Привет العربية 漢字 🧭', 5.5, 26+i*30);
                    });
                    c.globalCompositeOperation = 'multiply';
                    c.fillStyle = 'rgba(110,170,230,0.6)';
                    c.beginPath(); c.arc(230.25,150.5,70.75,0,Math.PI*2); c.fill();
                    const control = document.createElement('canvas').getContext('2d');
                    control.fillStyle = 'rgb(100,150,200)'; control.fillRect(0,0,1,1);
                    return {png:canvas.toDataURL(),
                        solid:Array.from(control.getImageData(0,0,1,1).data)};
                }""")
                assert result["solid"] == [100, 150, 200, 255], result["solid"]
                results.append({"font_control": configs.index(config), "args": args,
                    "hash": hashlib.sha256(result["png"].encode()).hexdigest()})
    record_property("isolated_canvas_control", json.dumps(results))
    assert len({r["hash"] for r in results}) == 1, results
