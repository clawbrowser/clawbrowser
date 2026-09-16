"""A raster group's requested family is not evidence of a local font lookup."""
import sys

import pytest

from conftest import _launch_browser_with_details
from canvas_network_mode import assert_canvas_network_mode, canvas_network_args


@pytest.mark.asyncio
@pytest.mark.parametrize("normalized", [False, True])
async def test_absent_font_labels_keep_generic_canvas_fallback(normalized, monkeypatch, record_property):
    if normalized and sys.platform != "linux":
        pytest.skip("normalized Canvas experiment is Linux-only")
    monkeypatch.setenv("CLAWBROWSER_QA_NORMALIZED_CANVAS", "1" if normalized else "0")
    extra = canvas_network_args()
    async with _launch_browser_with_details(
        fixture_name="valid_fingerprint.json", backend_mode="mock",
        skip_verify=True, headless=False, extra_browser_args=extra,
    ) as launch:
        canary = await assert_canvas_network_mode(launch["page"])
        record_property("canvas_mode", canary["mode"])
        rows = await launch["page"].evaluate("""async () => {
            const names = ['Abyssinica SIL', 'Papyrus', 'ClawbrowserMissingFontProbe'];
            const rows = [];
            const draw = (family, text) => {
                const canvas = document.createElement('canvas');
                canvas.width = 320; canvas.height = 64;
                const ctx = canvas.getContext('2d');
                ctx.fillStyle = 'white'; ctx.fillRect(0,0,320,64);
                ctx.fillStyle = 'black'; ctx.font = '23px ' + family;
                ctx.fillText(text, 3.25, 39);
                return {width:ctx.measureText(text).width,
                    pixels:ctx.getImageData(0,0,320,64).data};
            };
            for (const name of names) {
                let localLoaded = false;
                // local() addresses individual full/PostScript names, which
                // need not equal the CSS family name.
                for (const candidate of [name, name+' Regular', name.replaceAll(' ','')+'-Regular']) {
                    try {
                        await new FontFace('absence-control', 'local('+JSON.stringify(candidate)+')').load();
                        localLoaded = true;
                    } catch (_) {}
                }
                for (const generic of ['sans-serif','serif','monospace']) {
                    for (const text of ['Latin 0123', 'مرحبا بالعالم', '漢字 नमस्ते']) {
                        const baseline = draw(generic, text);
                        const probe = draw(JSON.stringify(name)+','+generic, text);
                        rows.push({name, generic, text, localLoaded,
                            equal:baseline.width === probe.width &&
                                baseline.pixels.every((v,i)=>v === probe.pixels[i]),
                            ink:baseline.pixels.some((v,i)=>i%4 !== 3 && v < 30),
                            width:baseline.width});
                    }
                }
            }
            return rows;
        }""")
        assert len(rows) == 27
        for row in rows:
            assert not row["localLoaded"], row
            assert row["ink"] and row["width"] > 0, row
            assert row["equal"], row


@pytest.mark.asyncio
@pytest.mark.skipif(sys.platform != 'linux', reason='Pinned Linux bundled-font metadata')
@pytest.mark.parametrize('normalized', [False, True])
@pytest.mark.parametrize('family', ['Noto Sans Kannada', 'Noto Sans Myanmar'])
async def test_bundled_fullname_face_matches_css_family(normalized, family, monkeypatch):
    monkeypatch.setenv('CLAWBROWSER_QA_NORMALIZED_CANVAS', '1' if normalized else '0')
    async with _launch_browser_with_details(
        fixture_name='valid_fingerprint.json', backend_mode='mock',
        skip_verify=True, headless=False, extra_browser_args=canvas_network_args(),
    ) as launch:
        await assert_canvas_network_mode(launch['page'])
        result = await launch['page'].evaluate('''async family => {
            const full=family+' Regular', postscript=family.replaceAll(' ','')+'-Regular';
            const faces=[];
            try {
                for (const [i,name] of [full,postscript].entries()) {
                    const face=new FontFace('qa-fullname-'+i,'local('+JSON.stringify(name)+')');
                    await face.load(); document.fonts.add(face); faces.push(face);
                }
                const draw=(family,text)=>{
                    const c=document.createElement('canvas');c.width=480;c.height=80;
                    const x=c.getContext('2d');x.fillStyle='white';x.fillRect(0,0,480,80);
                    x.fillStyle='black';x.font='23px '+family;x.fillText(text,3.25,39);
                    return {width:x.measureText(text).width,pixels:x.getImageData(0,0,480,80).data};
                };
                const rows=[];
                for(const generic of ['sans-serif','serif','monospace']) {
                    for(const text of ['Latin 0123','ಕನ್ನಡ မြန်မာ']) {
                        const baseline=draw(JSON.stringify(family)+','+generic,text);
                        for(const face of faces) {
                            const actual=draw(JSON.stringify(face.family)+','+generic,text);
                            rows.push({equal:baseline.width===actual.width &&
                                baseline.pixels.every((v,i)=>v===actual.pixels[i]),
                                ink:baseline.pixels.some((v,i)=>i%4!==3 && v<30),width:baseline.width});
                        }
                    }
                }
                return rows;
            } finally {for(const face of faces) document.fonts.delete(face);}
        }''', family)
        assert len(result) == 12
        assert all(row['equal'] and row['ink'] and row['width'] > 0 for row in result), result
