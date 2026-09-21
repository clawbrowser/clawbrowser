"""A local variable face in a nonzero TTC slot must retain its variation axes."""
import json
import sys

import pytest

from conftest import _launch_browser_with_details


@pytest.mark.asyncio
@pytest.mark.parametrize('region', ['JP', 'SC', 'KR', 'TC', 'HK'])
async def test_local_cjk_collection_preserves_weight_axis(record_property, region):
    if sys.platform not in ('linux', 'darwin', 'win32'):
        pytest.skip('desktop closed catalog')
    async with _launch_browser_with_details(
            fixture_name='valid_fingerprint.json', backend_mode='mock',
            skip_verify=True, headless=False) as launch:
        result = await launch['page'].evaluate('''async requestedFamily => {
          const face = new FontFace('CollectionProbe', 'local(' + JSON.stringify(requestedFamily) + ')',
                                    {weight: '100 900'});
          await face.load(); document.fonts.add(face);
          const ctx = document.createElement('canvas').getContext('2d');
          const rows = {};
          for (const family of ['CollectionProbe', requestedFamily]) {
            rows[family] = {};
            for (const weight of [100, 900]) {
              ctx.font = weight + ' 40px "' + family + '"';
              rows[family][weight] = ['Hamburgefontsiv0123', '漢字中文'].map(text => {
                const m = ctx.measureText(text);
                return [m.width, m.actualBoundingBoxLeft, m.actualBoundingBoxRight,
                        m.actualBoundingBoxAscent, m.actualBoundingBoxDescent];
              });
            }
          }
          document.fonts.delete(face);
          return {status: face.status, rows};
        }''', 'Noto Sans CJK ' + region)
    record_property('local_collection_variations', json.dumps(result))
    assert result['status'] == 'loaded'
    direct = result['rows']['Noto Sans CJK ' + region]
    local = result['rows']['CollectionProbe']
    assert direct['100'] != direct['900'], 'direct catalog variable control must change'
    assert local['100'] != local['900'], 'local TTC lookup silently ignored the weight axis'


@pytest.mark.asyncio
@pytest.mark.parametrize('region', ['JP', 'SC', 'KR', 'TC', 'HK'])
@pytest.mark.parametrize('context', ['document', 'worker'])
async def test_local_cjk_variation_mutation_invalidates_cached_face(record_property, region, context):
    if sys.platform not in ('linux', 'darwin', 'win32'):
        pytest.skip('desktop closed catalog')
    async with _launch_browser_with_details(
            fixture_name='valid_fingerprint.json', backend_mode='mock',
            skip_verify=True, headless=False) as launch:
        result = await launch['page'].evaluate('''async ({family, context}) => {
         const probe = async family => {
          const hasDocument = typeof document !== 'undefined';
          const fonts = hasDocument ? document.fonts : self.fonts;
          const makeContext = () => (hasDocument ? document.createElement('canvas') :
                                    new OffscreenCanvas(800, 100)).getContext('2d');
          const source = 'local(' + JSON.stringify(family) + ')';
          const face = new FontFace('MutableCollection', source,
            {weight: '100 900', variationSettings: '"wght" 100'});
          await face.load(); fonts.add(face);
          const canvas = makeContext();
          canvas.font = '400 40px MutableCollection';
          const span = hasDocument ? document.createElement('span') : {style: {}};
          span.style.cssText = 'font:400 40px MutableCollection;white-space:pre';
          span.textContent = 'Hamburgefontsiv0123漢字中文';
          if (hasDocument) document.body.append(span);
          const sample = () => {
            const fresh = makeContext();
            fresh.font = canvas.font;
            const metrics = ctx => {
              const m = ctx.measureText(span.textContent);
              return [m.width, m.actualBoundingBoxLeft, m.actualBoundingBoxRight,
                      m.actualBoundingBoxAscent, m.actualBoundingBoxDescent];
            };
            const range = hasDocument ? document.createRange() : null;
            if (range) range.selectNodeContents(span);
            return {warm: metrics(canvas), fresh: metrics(fresh),
                    dom: range ? range.getBoundingClientRect().width : null};
          };
          const before = sample();
          face.variationSettings = '"wght" 900';
          await fonts.ready;
          const changed = sample();
          const control = new FontFace('FreshCollection', source,
            {weight: '100 900', variationSettings: '"wght" 900'});
          await control.load(); fonts.add(control);
          canvas.font = '400 40px FreshCollection';
          span.style.fontFamily = 'FreshCollection';
          const expected = sample();
          canvas.font = '400 40px MutableCollection';
          span.style.fontFamily = 'MutableCollection';
          face.variationSettings = '"wght" 100';
          await fonts.ready;
          const restored = sample();
          fonts.delete(face); fonts.delete(control); if (hasDocument) span.remove();
          return {before, changed, expected, restored};
         };
         if (context === 'document') return probe(family);
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
             worker.postMessage(family);
           });
         } finally {worker.terminate(); URL.revokeObjectURL(url);}
        }''', {'family': 'Noto Sans CJK ' + region, 'context': context})
    record_property('local_collection_mutation', json.dumps(result))
    assert result['before'] != result['expected'], 'variation control must change'
    assert result['changed'] == result['expected'], 'mutated face retained stale metrics'
    assert result['restored'] == result['before'], 'restored axis retained stale metrics'
    for sample in result.values():
        assert sample['warm'] == sample['fresh'], 'warm/fresh canvas font cache differs'
