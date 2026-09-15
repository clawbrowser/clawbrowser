# macOS catalog ingestion experiment

This is a design probe, **not a fix for system fallback leakage**.

`scripts/probe_catalog_coretext.mm` reads explicit catalog font files through
`CTFontManagerCreateFontDescriptorsFromURL` and creates fonts from those
descriptors. It does not register or install fonts, and does not request an
automatic fallback. On the local QA Mac:

| File | CoreText result |
| --- | --- |
| Arimo-Regular.ttf | One face, Latin sample covered |
| NotoSansCJK-VF.otf.ttc | 35 descriptors, CJK sample covered |
| NotoColorEmoji.ttf | No descriptors |
| Lohit-Devanagari.ttf | One face, Devanagari sample covered |
| DejaVuSans.ttf | One face, Arabic sample covered |

The descriptor ordinal is not asserted to equal a raw TTC collection index;
named variations must be handled explicitly in a real catalog loader.

Chromium's `web_font_typeface_factory.cc` already routes CBDT/CBLC fonts through
Fontations. The opt-in `test_macos_catalog_ingestion.py` loads the exact bundled
Noto emoji file as a custom font through a local fixture HTTP proxy, then asks
CDP which font actually rendered the emoji. On candidate staged
`20260915-104649`, it **passed in 2.71s**, reporting custom Noto Color Emoji
glyphs, not Apple Color Emoji. This proves ingestion of this asset works in
the existing browser decoder, not that ordinary system fallback uses it.

Reproduce with `CLAWBROWSER_BINARY` pointing at the QA candidate and
`CLAWBROWSER_TEST_FONT_ASSET_ROOT` pointing at the pinned Chromium
`third_party/test_fonts/test_fonts` directory. The test skips unless both the
explicit assets and macOS are available.

Implementation still needs a validated, renderer-accessible catalog; explicit
family/style/collection selection; deterministic character/emoji fallback;
missing-glyph behavior; and coverage of worker/DOM/canvas. Merely registering
the files with CoreText would neither load this emoji asset nor establish a
closed fallback set. No sandbox relaxation or host-font installation is an
acceptable substitute.
