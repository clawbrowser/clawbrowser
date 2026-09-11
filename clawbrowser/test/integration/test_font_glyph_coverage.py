"""Coverage gate for shipped files, separate from renderer/shaping tests."""
import json
import sys
from pathlib import Path

import pytest
from fontTools.ttLib import TTCollection, TTFont
from conftest import _resolve_browser_binary


def test_bundled_catalog_has_multilingual_and_emoji_glyphs():
    if sys.platform != 'linux':
        pytest.skip('bundled Fontconfig catalog is Linux-only')
    binary = Path(_resolve_browser_binary()).resolve()
    manifest = json.loads((Path(__file__).parents[2] / 'fonts/catalog.json').read_text())
    fonts_dir = binary.parent / 'clawbrowser-fonts' / manifest['catalog_id'] / 'fonts'
    coverage = set()
    for entry in manifest['fonts']:
        path = fonts_dir / entry['file']
        faces = TTCollection(path).fonts if path.suffix == '.ttc' else [TTFont(path)]
        for face in faces:
            coverage.update(cp for cp, name in (face.getBestCmap() or {}).items()
                            if name != '.notdef')
            face.close()
    samples = ['Hello café română tiếng Việt', 'Привет Україна', 'Ελληνικά',
               'مرحبا بالعالم', 'שלום עולם', 'नमस्ते दुनिया', 'สวัสดีชาวโลก',
               '你好世界 日本語 한국어', 'বাংলা ভাষা', 'தமிழ் மொழி', 'తెలుగు భాష',
               'ಕನ್ನಡ ಭಾಷೆ', 'മലയാളം', 'ગુજરાતી', 'ਪੰਜਾਬੀ', 'සිංහල',
               'ភាសាខ្មែរ', 'မြန်မာဘာသာ', 'ქართული', 'Հայերեն', 'አማርኛ',
               '😀🙂🚀🧑💻👍🏽']
    missing = sorted({ord(char) for sample in samples for char in sample
                      if not char.isspace()} - coverage)
    assert not missing, ['U+%04X' % cp for cp in missing]
