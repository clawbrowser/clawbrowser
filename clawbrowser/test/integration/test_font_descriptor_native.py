"""Loaded webfont descriptors update under managed and native font policies."""
import json

import pytest

from conftest import _launch_browser_with_details
from test_dynamic_font_fallback import bundled_tinos_payload
from test_isolated_canvas_control import canvas_control_files


@pytest.mark.asyncio
@pytest.mark.parametrize('mode', ['native', 'managed'])
@pytest.mark.parametrize('descriptor,context', [
    (name, context) for name in ('sizeAdjust', 'ascentOverride', 'descentOverride')
    for context in ('document', 'worker')
] + [('lineGapOverride', 'document')])
async def test_webfont_metric_descriptor_mutation(tmp_path, record_property, mode, descriptor, context):
    payload = bundled_tinos_payload()
    fixture, mock = canvas_control_files(tmp_path, 'override')
    if mode == 'native':
        for path in (fixture, mock):
            data = json.loads(path.read_text())
            data['response']['fingerprint']['surface_policy']['fonts']['mode'] = 'native'
            path.write_text(json.dumps(data))
    async with _launch_browser_with_details(
        fixture_name=str(fixture), fingerprints_fixture_path=mock,
        backend_mode='mock', skip_verify=True, headless=False,
    ) as launch:
        result = await launch['page'].evaluate('''async ({payload, context, descriptor}) => {
          const probe = async ({payload, descriptor}) => {
            const fonts = typeof document === 'undefined' ? self.fonts : document.fonts;
            const bytes = Uint8Array.from(atob(payload), c => c.charCodeAt(0));
            const face = new FontFace('MutableWebFont', bytes, {[descriptor]: '50%'});
            await face.load(); fonts.add(face);
            const ctx = new OffscreenCanvas(800, 100).getContext('2d');
            const block = typeof document === 'undefined' ? null : document.createElement('div');
            if (block) {
              block.style.cssText = 'position:absolute;white-space:pre;font-size:40px;line-height:normal';
              block.textContent = 'Hamburgefontsiv0123\\nHamburgefontsiv0123';
              document.body.append(block);
            }
            const sample = family => {
              ctx.font = '40px ' + family;
              const fresh = new OffscreenCanvas(800, 100).getContext('2d');
              fresh.font = ctx.font;
              const metrics = c => {const m = c.measureText('Hamburgefontsiv0123');
                return [m.width, m.fontBoundingBoxAscent, m.fontBoundingBoxDescent];};
              if (block) block.style.fontFamily = family;
              return [metrics(ctx), metrics(fresh), block ? block.getBoundingClientRect().height : null];
            };
            const before = sample('MutableWebFont');
            face[descriptor] = '50%'; await fonts.ready;
            const unchanged = sample('MutableWebFont');
            let invalidError = null;
            try {face[descriptor] = 'not-a-percentage';}
            catch (error) {invalidError = error.name;}
            const afterInvalid = sample('MutableWebFont');
            face[descriptor] = '150%'; await fonts.ready;
            const changed = sample('MutableWebFont');
            const control = new FontFace('ControlWebFont', bytes, {[descriptor]: '150%'});
            await control.load(); fonts.add(control);
            const expected = sample('ControlWebFont');
            face[descriptor] = '50%'; await fonts.ready;
            const restored = sample('MutableWebFont');
            fonts.delete(face); fonts.delete(control);
            if (block) block.remove();
            return {before, unchanged, afterInvalid, invalidError, changed, expected, restored};
          };
          if (context === 'document') return probe({payload, descriptor});
          const url = URL.createObjectURL(new Blob([
            'onmessage=async e=>{try{postMessage({value:await (' + probe.toString() +
            ')(e.data)})}catch(e){postMessage({error:String(e)})}}'
          ], {type: 'text/javascript'}));
          const worker = new Worker(url);
          try {
            return await new Promise((resolve, reject) => {
              const timer = setTimeout(() => reject(new Error('worker timeout')), 10000);
              worker.onmessage = e => {clearTimeout(timer);
                e.data.error ? reject(new Error(e.data.error)) : resolve(e.data.value);};
              worker.onerror = e => {clearTimeout(timer); reject(new Error(e.message));};
              worker.postMessage({payload, descriptor});
            });
          } finally {worker.terminate(); URL.revokeObjectURL(url);}
        }''', {'payload': payload, 'context': context, 'descriptor': descriptor})
    record_property('webfont_descriptor_mutation', json.dumps(result))
    assert result['before'] != result['expected'], 'descriptor control must change'
    assert result['invalidError'] == 'SyntaxError'
    assert result['unchanged'] == result['before']
    assert result['afterInvalid'] == result['before']
    assert result['changed'] == result['expected'], 'descriptor cache is stale'
    assert result['restored'] == result['before'], 'restored descriptor cache is stale'
    for key in ('before', 'unchanged', 'afterInvalid', 'changed', 'expected', 'restored'):
        sample = result[key]
        assert sample[0] == sample[1], 'warm/fresh canvas metrics differ'
