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

## Explicit Fontations catalog reader

`CreateCatalogTypefaces` now consumes `LoadValidatedFontCatalog` snapshots and
instantiates each raw collection face and named variation directly from the
checked bytes. It does not consult the system font manager. The new separate
`clawbrowser_font_typeface_unittests` target checks malformed/empty input and,
when given the pinned catalog path and digest, actual emoji/Arabic/CJK glyphs
plus distinct CJK Regular/Bold named instances.

The initial implementation failed on CJK face 0, named instance 1:
`SkFontScanner_Fontations::scanInstance` passes the combined instance/face
index to `make_bridge_font_ref` without masking the high bits in this pinned
revision. The loader instead uses `SkTypeface_Make_Fontations`, whose existing
implementation separates the low collection bits and high instance bits,
then applies the named-instance coordinates. No upstream scanner modification
or omission of the failing variation was used.

Both tests pass on macOS against all 28 pinned files (49,709,560 bytes),
manifest SHA256 `7997598edceb7eab61bdbab53fa1ee923770ab349c4b8a5034c16b5dc540bbc7`.
Use `CLAWBROWSER_TEST_CATALOG_DIR` and `CLAWBROWSER_TEST_CATALOG_SHA256` for
this opt-in unit target. The same two tests also passed on Linux against the
catalog from the extracted QA archive.

The reader now exposes `MatchCatalogFamily` (case-insensitive catalog lookup
and Skia CSS3 style matching) and `MatchCatalogCharacter` (explicit ordered
families, glyph coverage, then CSS style). Unknown families, unsupported
characters and invalid Unicode scalar values return no match. Tests cover
regular/bold/italic, an absent host family, family ordering and emoji fallback.
These are scalar-character selection tests, not full shaping/variation-selector
or script-cluster acceptance.

**This reader is not yet connected to FontCache or automatic fallback.**
Family/style selection, bundle packaging, renderer cache lifetime/memory use
and missing-glyph behavior still need implementation and browser tests.
