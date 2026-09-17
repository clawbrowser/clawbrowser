"""Inline SVG text metrics must not recover fonts blocked in HTML layout.

This is not the SVG-image/Canvas raster test: SVG exposes per-character
positions, extents and substring lengths without reading any Canvas pixels.
"""
import json
import os
import sys

import pytest

from canvas_network_mode import assert_canvas_network_mode, canvas_network_args
from conftest import _launch_browser_with_details


@pytest.mark.asyncio
async def test_inline_svg_host_font_positive_control(record_property):
    configs = [os.environ.get('CLAWBROWSER_TEST_FONTCONFIG_' + s) for s in ('A', 'B')]
    if sys.platform != 'linux' or not all(configs):
        pytest.skip('requires two Linux host-font controls')
    observations = []
    for config in configs:
        async with _launch_browser_with_details(
            fixture_name=None, backend_mode='vanilla', skip_verify=True,
            headless=False, extra_env={'FONTCONFIG_FILE': config},
        ) as launch:
            metrics = await launch['page'].evaluate('''() => {
                const ns='http://www.w3.org/2000/svg';
                const svg=document.createElementNS(ns,'svg');
                const text=document.createElementNS(ns,'text');
                text.textContent='Latin ffi AV 0123';text.style.fontSize='23px';
                svg.append(text);document.body.append(svg);
                try {return ['sans-serif','serif','monospace'].map(family=>{
                    text.style.fontFamily=family;
                    return text.getComputedTextLength();
                });} finally {svg.remove();}
            }''')
            assert len(metrics) == 3 and all(v > 0 for v in metrics), metrics
            observations.append(metrics)
    assert observations[0] != observations[1], 'No live host-font control; isolation inconclusive'
    record_property('svg_host_font_positive_control', json.dumps(observations))


@pytest.mark.asyncio
@pytest.mark.parametrize('normalized', [False, True])
async def test_inline_svg_blocked_font_metrics(normalized, monkeypatch, record_property):
    configs = [os.environ.get('CLAWBROWSER_TEST_FONTCONFIG_' + s) for s in ('A', 'B')]
    if sys.platform != 'linux' or not all(configs):
        pytest.skip('requires two Linux host-font controls')
    monkeypatch.setenv('CLAWBROWSER_QA_NORMALIZED_CANVAS', '1' if normalized else '0')
    observations = []
    for config in configs:
        async with _launch_browser_with_details(
            fixture_name='valid_fingerprint.json', backend_mode='mock',
            skip_verify=True, headless=False, extra_env={'FONTCONFIG_FILE': config},
            extra_browser_args=canvas_network_args(),
        ) as launch:
            await assert_canvas_network_mode(launch['page'])
            rows = await launch['page'].evaluate('''async () => {
                const ns='http://www.w3.org/2000/svg';
                const svg=document.createElementNS(ns,'svg');
                svg.setAttribute('width','800');svg.setAttribute('height','120');
                const node=document.createElementNS(ns,'text');
                node.setAttribute('x','3.25');node.setAttribute('y','70');svg.append(node);
                const span=document.createElement('span');
                span.style.cssText='display:inline-block;white-space:pre;font-size:23px';
                document.body.append(svg,span);
                const rect=r=>[r.x,r.y,r.width,r.height];
                const measure=(family,text,style)=>{
                    for(const n of [node,span]) {
                        n.style.fontFamily=family;n.style.fontSize='23px';
                        n.style.fontWeight=style==='normal'?'400':'700';
                        n.style.fontStyle=style==='normal'?'normal':'italic';
                        n.textContent=text;
                    }
                    const count=node.getNumberOfChars();
                    return {count,length:node.getComputedTextLength(),bbox:rect(node.getBBox()),
                        htmlWidth:span.getBoundingClientRect().width,
                        characters:Array.from({length:count},(_,i)=>{
                            const a=node.getStartPositionOfChar(i),b=node.getEndPositionOfChar(i);
                            return [a.x,a.y,b.x,b.y,...rect(node.getExtentOfChar(i)),
                                node.getSubStringLength(i,1),node.getRotationOfChar(i)];
                        })};
                };
                const rows=[];
                try {
                    for(const name of ['Papyrus','Liberation Sans','ClawbrowserMissingFontProbe']) {
                        let loaded=false;
                        try {await new FontFace('qa-svg-local','local('+JSON.stringify(name)+')').load();loaded=true;}
                        catch (_) {}
                        for(const generic of ['sans-serif','serif','monospace'])
                            for(const text of ['Latin ffi AV 0123','مرحبا بالعالم','漢字 नमस्ते'])
                                for(const style of ['normal','bold-italic']) {
                                    const baseline=measure(generic,text,style);
                                    const probe=measure(JSON.stringify(name)+','+generic,text,style);
                                    rows.push({name,generic,text,style,loaded,baseline,
                                        equal:JSON.stringify(baseline)===JSON.stringify(probe)});
                                }
                    }
                    return rows;
                } finally {svg.remove();span.remove();}
            }''')
            assert len(rows) == 54
            for row in rows:
                assert not row['loaded'], row
                assert row['baseline']['count'] > 0, row
                assert row['baseline']['length'] > 0 and row['baseline']['htmlWidth'] > 0, row
                assert row['baseline']['bbox'][2] > 0 and row['baseline']['bbox'][3] > 0, row
                assert row['equal'], row
            # Reject a vacuous implementation returning the same metrics for
            # every generic family, rather than actually laying out text.
            widths = {r['baseline']['length'] for r in rows
                      if r['text'] == 'Latin ffi AV 0123' and r['style'] == 'normal'}
            assert len(widths) > 1, rows
            observations.append(rows)
    assert observations[0] == observations[1], 'Host font configuration changed SVG metrics'
    record_property('svg_font_metrics', json.dumps({
        'normalized': normalized, 'font_controls': 2, 'comparisons': 108,
        'per_character_metrics': True, 'host_control_equal': True,
    }))
