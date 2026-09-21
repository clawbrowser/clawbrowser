"""Cross-context fidelity for supported Canvas color spaces and pixel formats."""
import hashlib
import json
import sys

import pytest

from conftest import _launch_browser_with_details
from test_isolated_canvas_control import canvas_control_files


@pytest.mark.asyncio
@pytest.mark.parametrize("mode", ["native", "override", "normalized"])
async def test_canvas_color_formats_across_contexts(tmp_path, record_property, mode):
    if mode == "normalized" and sys.platform != "linux":
        pytest.skip("normalized canvas experiment is Linux-only")
    fixture, mock = canvas_control_files(tmp_path, "override" if mode == "normalized" else mode)
    async with _launch_browser_with_details(
        fixture_name=str(fixture), backend_mode="mock", skip_verify=True,
        headless=False, fingerprints_fixture_path=mock,
        extra_browser_args=("--clawbrowser-experimental-normalized-canvas",)
            if mode == "normalized" else (),
    ) as launch:
        results = await launch["page"].evaluate(r"""async () => {
            function render(canvas, settings) {
                canvas.width=96; canvas.height=64;
                const c=canvas.getContext('2d',settings);
                const attributes=c.getContextAttributes();
                const g=c.createLinearGradient(.3,.7,91.5,60.25);
                g.addColorStop(0,'color(display-p3 0.9 0.2 0.1 / 0.7)');
                g.addColorStop(1,'color(display-p3 0.1 0.8 0.6 / 0.9)');
                c.fillStyle=g; c.fillRect(0,0,96,64);
                c.globalCompositeOperation='screen';
                c.fillStyle='rgba(79,131,233,.51)';
                c.beginPath(); c.ellipse(42.5,31.25,29.75,17.125,.2,0,Math.PI*2); c.fill();
                const images={};
                for(const pixelFormat of ['rgba-unorm8','rgba-float16']) {
                    const options={colorSpace:settings.colorSpace,pixelFormat};
                    const a=c.getImageData(0,0,96,64,options);
                    const b=c.getImageData(0,0,96,64,options);
                    const bytes=Array.from(new Uint8Array(a.data.buffer,a.data.byteOffset,a.data.byteLength));
                    const again=new Uint8Array(b.data.buffer,b.data.byteOffset,b.data.byteLength);
                    images[pixelFormat]={colorSpace:a.colorSpace,pixelFormat:a.pixelFormat,
                        arrayType:a.data.constructor.name,finite:Array.from(a.data).every(Number.isFinite),
                        stable:bytes.every((v,i)=>v===again[i]),bytes};
                }
                return {attributes,images};
            }
            const results=[];
            for(const colorSpace of ['srgb','display-p3']) {
                for(const colorType of ['unorm8','float16']) {
                    for(const willReadFrequently of [false,true]) {
                        const settings={colorSpace,colorType,willReadFrequently};
                        const dom=render(document.createElement('canvas'),settings);
                        const offscreen=render(new OffscreenCanvas(96,64),settings);
                        const url=URL.createObjectURL(new Blob([
                            'onmessage=e=>postMessage(('+render.toString()+')(new OffscreenCanvas(96,64),e.data))'
                        ],{type:'text/javascript'}));
                        const worker=new Worker(url); let timer;
                        try {
                            const data=await new Promise((resolve,reject)=>{
                                timer=setTimeout(()=>reject(new Error('color worker timeout')),10000);
                                worker.onmessage=e=>resolve(e.data);
                                worker.onerror=()=>reject(new Error('color worker failed'));
                                worker.postMessage(settings);
                            });
                            results.push({settings,contexts:{dom,offscreen,worker:data}});
                        } finally {clearTimeout(timer);worker.terminate();URL.revokeObjectURL(url);}
                    }
                }
            }
            return results;
        }""")
    observations = []
    groups = {}
    for row in results:
        settings = row["settings"]
        for context, value in row["contexts"].items():
            assert value["attributes"]["colorSpace"] == settings["colorSpace"]
            assert value["attributes"]["colorType"] == settings["colorType"]
            for requested, image in value["images"].items():
                assert image["colorSpace"] == settings["colorSpace"]
                assert image["pixelFormat"] == requested
                assert image["stable"] and image["finite"]
                floating = requested == "rgba-float16"
                assert image["arrayType"] == ("Float16Array" if floating else "Uint8ClampedArray")
                assert len(image["bytes"]) == 96 * 64 * (8 if floating else 4)
                assert len(set(image["bytes"])) > 32
                digest = hashlib.sha256(bytes(image["bytes"])).hexdigest()
                key = (settings["colorSpace"], settings["colorType"], requested)
                groups.setdefault(key, set()).add(digest)
                observations.append({**settings,"context":context,"readFormat":requested,"hash":digest})
    record_property("color_format_matrix", json.dumps({"mode":mode,"observations":observations}))
    assert len(results) == 8 and len(observations) == 48
    assert all(len(hashes) == 1 for hashes in groups.values()), observations
