"""A Windows profile on a Windows host renders with default Windows fonts.

Only families that every Windows 10/11 install ships may be visible. The
bundled Linux catalog and any non-default host font must stay invisible.
Set CLAWBROWSER_EXTRA_HOST_FONT to the family name of a font installed on the
test host outside the Windows defaults to also check that it stays hidden.
"""
import os
import sys

import pytest

from conftest import _launch_browser_with_details

pytestmark = pytest.mark.skipif(
    sys.platform != 'win32', reason='Windows host font policy')

PROBE_JS = '''(families) => {
  const ctx = document.createElement('canvas').getContext('2d');
  const sample = 'mmmmmmmmmmlli WWWW 0123456789 Hamburgefontsiv';
  const measure = (font) => {
    ctx.font = font;
    const m = ctx.measureText(sample);
    return [m.width, m.actualBoundingBoxAscent, m.actualBoundingBoxDescent];
  };
  const result = {};
  for (const family of families) {
    // A missing family falls back to the base, so it never changes metrics.
    // A present one differs from at least one base (Arial matches the
    // Windows sans-serif default, Times New Roman the serif one).
    result[family] = ['monospace', 'serif', 'sans-serif'].some(base =>
      JSON.stringify(measure(`48px "${family}", ${base}`)) !==
      JSON.stringify(measure(`48px ${base}`)));
  }
  return result;
}'''

INK_JS = '''(texts) => {
  const ctx = document.createElement('canvas').getContext('2d');
  ctx.font = '48px sans-serif';
  // Tofu boxes have ink too; compare against an unassigned code point.
  const tofu = ctx.measureText('\\u{10FFFD}').width;
  return texts.map(text => {
    const m = ctx.measureText(text);
    return {text, width: m.width, tofu,
            ink: m.actualBoundingBoxRight + m.actualBoundingBoxLeft};
  });
}'''


def _launch():
    return _launch_browser_with_details(
        fixture_name='valid_fingerprint_windows.json', backend_mode='mock',
        skip_verify=True, headless=False)


@pytest.mark.asyncio
async def test_windows_profile_sees_default_windows_fonts_only():
    extra = os.environ.get('CLAWBROWSER_EXTRA_HOST_FONT', '')
    hidden = ['Arimo', 'Tinos', 'Cousine', 'DejaVu Sans', 'Noto Color Emoji',
              'Segoe UI Variable', 'Meiryo'] + ([extra] if extra else [])
    visible = ['Arial', 'Segoe UI', 'Calibri', 'Cambria', 'Consolas',
               'Times New Roman', 'Courier New']
    async with _launch() as launch:
        fingerprint = launch['fingerprint_data']['response']['fingerprint']
        assert fingerprint['os'] == 'windows', fingerprint['os']
        rows = await launch['page'].evaluate(PROBE_JS, visible + hidden)
    assert all(rows[name] for name in visible), rows
    assert not any(rows[name] for name in hidden), rows


@pytest.mark.asyncio
async def test_windows_profile_system_ui_is_segoe_ui():
    async with _launch() as launch:
        rows = await launch['page'].evaluate('''() => {
          const ctx = document.createElement('canvas').getContext('2d');
          const out = {};
          for (const family of ['system-ui', '"Segoe UI"', 'Arimo', 'Arial']) {
            ctx.font = '32px ' + family;
            const m = ctx.measureText('Hamburgefontsiv 0123 Привет');
            out[family] = [m.width, m.actualBoundingBoxAscent,
                           m.actualBoundingBoxDescent];
          }
          return out;
        }''')
    assert rows['system-ui'] == rows['"Segoe UI"'], rows
    assert rows['system-ui'] != rows['Arial'], rows


@pytest.mark.asyncio
async def test_windows_profile_fallback_renders_emoji_and_scripts():
    texts = ['\U0001F600', '中文', '日本語',
             '한국어', 'हिन्दी',
             'ภาษาไทย',
             'العربية']
    async with _launch() as launch:
        rows = await launch['page'].evaluate(INK_JS, texts)
    for row in rows:
        assert row['ink'] > 0, row
        # Two or more glyphs of real coverage never match one tofu box.
        assert row['width'] != row['tofu'] * len(row['text']), row


@pytest.mark.asyncio
async def test_extra_host_font_is_visible_without_a_profile():
    # Negative control: the hidden check above only proves something when an
    # unprotected browser on the same host does see the extra font.
    extra = os.environ.get('CLAWBROWSER_EXTRA_HOST_FONT', '')
    if not extra:
        pytest.skip('CLAWBROWSER_EXTRA_HOST_FONT is not set')
    async with _launch_browser_with_details(
            backend_mode='vanilla', headless=False) as launch:
        rows = await launch['page'].evaluate(PROBE_JS, [extra, 'Segoe UI'])
    assert rows == {extra: True, 'Segoe UI': True}, rows
