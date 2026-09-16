"""FontFaceSet invalidation must not escape the managed character catalog."""
import base64
import hashlib
import json
from pathlib import Path
import sys

import pytest

from conftest import _launch_browser_with_details, _resolve_browser_binary
from test_complex_font_fallback import platform_fonts

pytestmark = pytest.mark.skipif(sys.platform not in ("linux", "darwin"),
                                reason="Linux/macOS bundled catalog")


def bundled_tinos_payload():
    manifest = json.loads((Path(__file__).parents[2] / "fonts/catalog.json").read_text())
    binary = Path(_resolve_browser_binary()).resolve()
    search_root = binary.parent if sys.platform == "linux" else binary.parents[1]
    candidates = list(search_root.rglob("Tinos-Regular.ttf"))
    expected = next(f["sha256"] for f in manifest["fonts"] if f["file"] == "Tinos-Regular.ttf")
    assert candidates, "Test requires the actual packaged font asset"
    data = candidates[0].read_bytes()
    assert hashlib.sha256(data).hexdigest() == expected
    return base64.b64encode(data).decode("ascii")


@pytest.mark.asyncio
async def test_unicode_range_webfont_load_and_removal_preserve_fallback(record_property):
    payload = bundled_tinos_payload()
    samples = ["مرحبا", "नमस्ते", "สวัสดี", "👩\u200d💻"]
    async with _launch_browser_with_details(
        fixture_name="valid_fingerprint.json", backend_mode="mock",
        skip_verify=True, headless=False,
    ) as launch:
        page = launch["page"]
        await page.set_content('<meta charset="utf-8"><style>span{font:32px '
                               'WebLatin,sans-serif;display:inline-block}</style>'
                               '<span id="latin">Wide Latin WWW iii 123</span>' +
                               ''.join(f'<span id="s{i}">{text}</span>'
                                       for i, text in enumerate(samples)))

        async def snapshot():
            await page.evaluate("document.fonts.ready")
            result = {}
            for name in ["latin", "s0", "s1", "s2", "s3"]:
                fonts = await platform_fonts(page, "#" + name)
                result[name] = {
                    "fonts": sorted((f["familyName"], f.get("postScriptName", ""),
                                     f["glyphCount"], f["isCustomFont"]) for f in fonts),
                    "width": await page.locator("#" + name).evaluate(
                        "el => el.getBoundingClientRect().width"),
                }
                assert result[name]["fonts"] and result[name]["width"] > 0
            return result

        before = await snapshot()
        assert not any(f[3] for item in before.values() for f in item["fonts"])
        await page.evaluate('''async payload => {
            const bytes = Uint8Array.from(atob(payload), c => c.charCodeAt(0));
            window.qaFace = new FontFace('WebLatin', bytes, {unicodeRange:'U+0000-007F'});
            await qaFace.load(); document.fonts.add(qaFace);
        }''', payload)
        loaded = await snapshot()
        assert any(f[3] and f[2] > 0 for f in loaded["latin"]["fonts"]), loaded
        assert loaded["latin"]["width"] != before["latin"]["width"], loaded
        for name in ["s0", "s1", "s2", "s3"]:
            assert loaded[name] == before[name], (name, before[name], loaded[name])
        removed = await page.evaluate("document.fonts.delete(qaFace)")
        assert removed
        after = await snapshot()
        assert after == before, (before, after)
    record_property("dynamic_font_fallback", json.dumps({
        "platform": sys.platform, "before": before, "loaded": loaded, "after": after,
    }))


@pytest.mark.asyncio
@pytest.mark.parametrize("persistent_context", [False, True], ids=["fresh", "persistent"])
@pytest.mark.parametrize("removal", ["delete", "clear"])
async def test_dynamic_webfont_canvas_matches_worker(record_property, persistent_context, removal):
    async with _launch_browser_with_details(
        fixture_name="valid_fingerprint.json", backend_mode="mock",
        skip_verify=True, headless=False,
    ) as launch:
        observations = await launch["page"].evaluate('''async ({payload, persistent, removal}) => {
            async function probe(payload, kind, persistent, removal) {
                async function bounded(promise, stage) {
                    let timer;
                    try {return await Promise.race([promise,new Promise((_,reject)=>{
                        timer=setTimeout(()=>reject(new Error(kind+': '+stage+' timeout')),3000);
                    })]);} finally {clearTimeout(timer);}
                }
                async function readyDiagnostic() {
                    let timer;
                    try {return await Promise.race([fontSet.ready.then(()=>true),new Promise(resolve=>{
                        timer=setTimeout(()=>resolve(false),3000);
                    })]);} finally {clearTimeout(timer);}
                }
                const fontSet = typeof document === 'undefined' ? self.fonts : document.fonts;
                const texts = ['Latin WWW iii', 'مرحبا', 'नमस्ते', 'สวัสดี', '👩‍💻'];
                function makeContext() {
                    const canvas = kind === 'dom' ? document.createElement('canvas') : new OffscreenCanvas(256,64);
                    canvas.width=256; canvas.height=64;
                    const ctx=canvas.getContext('2d', {willReadFrequently:true});
                    ctx.font='28px WebLatin,sans-serif';
                    return ctx;
                }
                // Keep both the context and its font assignment across mutations.
                let retainedContext;
                async function capture() {
                    const rows = [];
                    for (const text of texts) {
                        const ctx = persistent ? (retainedContext ||= makeContext()) : makeContext();
                        ctx.fillStyle='white'; ctx.fillRect(0,0,256,64);
                        ctx.fillStyle='black';
                        ctx.fillText(text,8,40);
                        const metrics=ctx.measureText(text);
                        const pixels=ctx.getImageData(0,0,256,64).data;
                        const hash=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',pixels)),
                            v=>v.toString(16).padStart(2,'0')).join('');
                        rows.push({text,width:metrics.width,left:metrics.actualBoundingBoxLeft,
                            right:metrics.actualBoundingBoxRight,ascent:metrics.actualBoundingBoxAscent,
                            descent:metrics.actualBoundingBoxDescent,hash});
                    }
                    return rows;
                }
                const before=kind==='worker-fresh'?null:await capture();
                const face=new FontFace('WebLatin',Uint8Array.from(atob(payload),c=>c.charCodeAt(0)),
                    {unicodeRange:'U+0000-007F'});
                    await bounded(face.load(),'face.load'); fontSet.add(face);
                    // Record both readiness and rendering so a stuck promise
                    // cannot hide a separate stale fallback-cache failure.
                    const readyAfterAdd=await readyDiagnostic();
                const loaded=await capture();
                    const hadFace=fontSet.has(face);
                    if (removal === 'clear') fontSet.clear();
                    else fontSet.delete(face);
                    const removed=hadFace && !fontSet.has(face);
                    const readyAfterDelete=await readyDiagnostic();
                const after=await capture();
                return {before:before||after,loaded,after,removed,readyAfterAdd,readyAfterDelete};
            }
            const dom=await probe(payload,'dom',persistent,removal);
            const offscreen=await probe(payload,'offscreen',persistent,removal);
            async function workerProbe(kind) {
              const url=URL.createObjectURL(new Blob([
                'onmessage=async e=>{try{postMessage({result:await ('+probe.toString()+')(e.data.payload,e.data.kind,e.data.persistent,e.data.removal)})}' +
                'catch(error){postMessage({error:String(error)})}}'
            ],{type:'text/javascript'}));
            const worker=new Worker(url); let timer;
            try {
                const result=await new Promise((resolve,reject)=>{
                    timer=setTimeout(()=>reject(new Error('font worker timeout')),15000);
                    worker.onerror=e=>reject(new Error(e.message));
                    worker.onmessage=e=>e.data.error?reject(new Error(e.data.error)):resolve(e.data.result);
                    worker.postMessage({payload,kind,persistent,removal});
                });
                return result;
            } finally {clearTimeout(timer);worker.terminate();URL.revokeObjectURL(url);}
            }
            return {dom,offscreen,worker:await workerProbe('worker-warm'),workerFresh:await workerProbe('worker-fresh')};
        }''', {"payload": bundled_tinos_payload(), "persistent": persistent_context,
               "removal": removal})
    record_property("dynamic_font_canvas", json.dumps(observations))
    for kind, states in observations.items():
        assert states["readyAfterAdd"] and states["readyAfterDelete"], kind
        assert states["removed"]
        assert states["before"] == states["after"], kind
        assert states["before"][0]["width"] != states["loaded"][0]["width"], kind
        assert states["before"][0]["hash"] != states["loaded"][0]["hash"], kind
        assert states["before"][1:] == states["loaded"][1:], kind
    for state in ["before", "loaded", "after"]:
        assert observations["dom"][state] == observations["offscreen"][state] == observations["worker"][state] == observations["workerFresh"][state], state
