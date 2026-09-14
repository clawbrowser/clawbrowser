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


def canvas_control_files(tmp_path, mode="native"):
    fixture = json.loads((FIXTURE_DIR / "valid_fingerprint.json").read_text())
    fixture["response"]["fingerprint"]["surface_policy"]["canvas"]["mode"] = mode
    path = tmp_path / "native-canvas-control.json"
    path.write_text(json.dumps(fixture))
    mock = tmp_path / "native-canvas-api.json"
    mock.write_text(json.dumps({
        "request": {k: v for k, v in fixture["request"].items()
                    if not k.startswith("runtime_")},
        "response": fixture["response"],
    }))
    return path, mock


@pytest.mark.asyncio
async def test_isolated_native_canvas_across_font_and_raster_controls(tmp_path, record_property):
    configs = [os.environ.get("CLAWBROWSER_TEST_FONTCONFIG_" + x) for x in ("A", "B")]
    if sys.platform != "linux" or not all(configs):
        pytest.skip("requires two Linux host-font controls")
    # Native cached canvas triggers privacy regeneration. Return the control
    # policy through the mock API too, matching preserved cached targeting.
    path, mock = canvas_control_files(tmp_path)
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


@pytest.mark.asyncio
@pytest.mark.parametrize("canvas_mode", ["native", "override"])
async def test_canvas_window_offscreen_worker_controls(tmp_path, record_property, canvas_mode):
    configs = [os.environ.get("CLAWBROWSER_TEST_FONTCONFIG_" + x) for x in ("A", "B")]
    if sys.platform != "linux" or not all(configs):
        pytest.skip("requires two Linux host-font controls")
    path, mock = canvas_control_files(tmp_path, canvas_mode)
    observations = []
    for config in configs:
        for args in ((), ("--disable-skia-runtime-opts",)):
            async with _launch_browser_with_details(
                fixture_name=str(path), backend_mode="mock", skip_verify=True,
                headless=False, extra_env={"FONTCONFIG_FILE": config},
                extra_browser_args=args, fingerprints_fixture_path=mock,
            ) as launch:
                result = await launch["page"].evaluate(r"""async () => {
                    function render(canvas, frequent) {
                        canvas.width=192; canvas.height=128;
                        const c=canvas.getContext('2d', {willReadFrequently:frequent});
                        const gradient=c.createLinearGradient(.25,.75,180.5,110.25);
                        gradient.addColorStop(0,'#183654'); gradient.addColorStop(1,'#dbaf67');
                        c.fillStyle=gradient; c.fillRect(0,0,192,128);
                        c.save(); c.translate(11.125,9.375); c.rotate(.137);
                        c.fillStyle='rgba(210,32,89,.71)';
                        c.beginPath(); c.moveTo(3.2,11.7);
                        c.bezierCurveTo(97.2,-20.8,154.9,130.1,171.4,68.6);
                        c.lineTo(18.8,92.2); c.closePath(); c.fill(); c.restore();
                        c.globalCompositeOperation='multiply';
                        c.font='17px Arimo'; c.fillStyle='#395ac7';
                        c.fillText('Latin 0123 Привет',3.25,43.75);
                        c.font='16px sans-serif';
                        c.fillText('العربية 漢字 🧭',4.5,78.25);
                        c.globalCompositeOperation='source-over';
                        c.shadowColor='rgba(20,90,180,.4)'; c.shadowBlur=3.5;
                        c.strokeStyle='#65deab'; c.lineWidth=1.25;
                        c.beginPath(); c.ellipse(91.25,86.75,33.5,17.25,.23,0,Math.PI*2); c.stroke();
                        return Array.from(c.getImageData(0,0,192,128).data);
                    }
                    const results={};
                    for (const frequent of [false,true]) {
                    const label=frequent?'readback':'default';
                    results[label+'-dom']=render(document.createElement('canvas'),frequent);
                    results[label+'-offscreen']=render(new OffscreenCanvas(192,128),frequent);
                    const url=URL.createObjectURL(new Blob([
                        'onmessage=e=>postMessage(('+render.toString()+')(new OffscreenCanvas(192,128),e.data))'
                    ],{type:'text/javascript'}));
                    const worker=new Worker(url);
                    let timer;
                    try {
                        const pixels=await new Promise((resolve,reject)=>{
                            timer=setTimeout(()=>reject(new Error('worker canvas timeout')),10000);
                            worker.onmessage=e=>resolve(e.data);
                            worker.onerror=()=>reject(new Error('worker canvas failed'));
                            worker.postMessage(frequent);
                        });
                        results[label+'-worker']=pixels;
                    } finally {clearTimeout(timer); worker.terminate(); URL.revokeObjectURL(url);}
                    }
                    return results;
                }""")
                assert len(result) == 6
                for pixels in result.values():
                    assert len(pixels) == 192 * 128 * 4
                    assert len(set(pixels)) > 64, "Control drawing must not be empty or uniform"
                hashes = {key: hashlib.sha256(bytes(pixels)).hexdigest()
                          for key, pixels in result.items()}
                observations.append({"font_control": configs.index(config), "args": args,
                                     "hashes": hashes})
    record_property("cross_context_control", json.dumps({"mode": canvas_mode, "observations": observations}))
    assert len({value for row in observations for value in row["hashes"].values()}) == 1, observations
