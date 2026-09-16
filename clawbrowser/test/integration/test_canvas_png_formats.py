"""PNG export agreement; do not assume float16 PNG is lossless."""
import hashlib
import json
import sys

import pytest

from conftest import _launch_browser_with_details
from test_isolated_canvas_control import canvas_control_files


@pytest.mark.asyncio
@pytest.mark.parametrize("mode", ["native", "override", "normalized"])
async def test_png_formats_across_contexts(tmp_path, record_property, mode):
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
            async function render(canvas, settings) {
                canvas.width=64; canvas.height=32;
                const c=canvas.getContext('2d',settings);
                for(let i=0;i<8;i++) {
                    c.fillStyle=`color(display-p3 .8 .3 .7 / ${i/7})`;
                    c.fillRect(i*8,0,8,32);
                }
                const read=()=>Array.from(c.getImageData(0,0,64,32).data);
                const before=read();
                const exportPng=()=>canvas.convertToBlob
                    ? canvas.convertToBlob({type:'image/png'})
                    : new Promise((resolve,reject)=>canvas.toBlob(
                        b=>b?resolve(b):reject(new Error('empty PNG')), 'image/png'));
                const blob=await exportPng(), again=await exportPng();
                const png=Array.from(new Uint8Array(await blob.arrayBuffer()));
                const repeated=new Uint8Array(await again.arrayBuffer());
                const bitmap=await createImageBitmap(blob);
                const decoded=new OffscreenCanvas(64,32);
                const d=decoded.getContext('2d',settings);
                d.drawImage(bitmap,0,0); bitmap.close();
                const after=read(), pixels=Array.from(d.getImageData(0,0,64,32).data);
                return {attributes:c.getContextAttributes(),mime:blob.type,png,pixels,
                    stable:png.length===repeated.length && png.every((v,i)=>v===repeated[i]),
                    unchanged:before.every((v,i)=>v===after[i]),
                    alpha:before.filter((_,i)=>i%4===3),
                    decodedAlpha:pixels.filter((_,i)=>i%4===3)};
            }
            const rows=[];
            for(const colorSpace of ['srgb','display-p3']) {
                for(const colorType of ['unorm8','float16']) {
                    const settings={colorSpace,colorType};
                    const dom=await render(document.createElement('canvas'),settings);
                    const offscreen=await render(new OffscreenCanvas(64,32),settings);
                    const url=URL.createObjectURL(new Blob([
                        'onmessage=async e=>postMessage(await ('+render.toString()+')(new OffscreenCanvas(64,32),e.data))'
                    ],{type:'text/javascript'}));
                    const worker=new Worker(url); let timer;
                    try {
                        const data=await new Promise((resolve,reject)=>{
                            timer=setTimeout(()=>reject(new Error('PNG worker timeout')),10000);
                            worker.onmessage=e=>resolve(e.data);
                            worker.onerror=()=>reject(new Error('PNG worker failed'));
                            worker.postMessage(settings);
                        });
                        rows.push({settings,contexts:{dom,offscreen,worker:data}});
                    } finally {clearTimeout(timer);worker.terminate();URL.revokeObjectURL(url);}
                }
            }
            return rows;
        }""")
    observations = []
    for row in results:
        hashes = set()
        for context, value in row["contexts"].items():
            assert all(value["attributes"][k] == v for k, v in row["settings"].items())
            assert value["mime"] == "image/png"
            assert value["png"][:8] == [137, 80, 78, 71, 13, 10, 26, 10]
            assert value["stable"] and value["unchanged"]
            assert len(value["pixels"]) == 64 * 32 * 4
            assert value["alpha"] == value["decodedAlpha"]
            assert len(set(value["alpha"])) == 8
            assert min(value["alpha"]) == 0 and max(value["alpha"]) == 255
            digest = hashlib.sha256(bytes(value["pixels"])).hexdigest()
            hashes.add(digest)
            observations.append({**row["settings"], "context": context, "hash": digest})
        assert len(hashes) == 1, observations
    record_property("png_format_matrix", json.dumps({"mode": mode, "observations": observations}))
    assert len(results) == 4 and len(observations) == 12
