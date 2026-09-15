# TTC variation clone regression

The first browser raster test failed on the previous `20260915-135316`
candidate: CSS `font-weight:100` and `font-variation-settings:"wght" 100`
rendered different CJK SC glyphs, with synthetic styles disabled. Returning
to the first style reproduced the first image, excluding a one-off repaint.

Two source defects were involved:

1. Blink's custom platform-data helper passed the default collection index 0
   when cloning a selected nonzero TTC face. Fontations rejects mismatched
   indices and silently returns the original typeface, ignoring the axes.
2. Even with an explicit matching index, Fontations' fused clone arguments
   omitted that index. Reconstructing the face therefore selected TTC face 0.

Patch 040 forwards the actual catalog face index through Blink's variation
and palette clone requests and preserves it in Fontations' fused arguments.
The default remains zero for existing non-catalog callers. No font selection
through the host manager or site-specific exception was introduced.

A C++ regression reproduced the lost CJK SC family before the Skia fix, then
passed on macOS and Linux: cloning to weight 675 retains both family and
weight. The browser regression on candidate `20260915-143518` passed for all
five CJK families (JP/KR/SC/TC/HK): weights 100, 400 and 900 produce distinct
images; equivalent explicit-axis images match each weight, including a repeat
control. Catalog provenance and complex-script/emoji tests also passed:
8 tests, 25.89 seconds. This is specific axis/raster evidence, not all possible
variation axes, palettes, shaping clusters or memory acceptance.

The patch pipeline now restores Skia-owned files through the nested checkout
instead of treating them as absent Chromium-root files. Both remote scripts
are covered by a temporary nested-repository regression; script tests pass.

Mac launcher SHA256:
`77a2462abe5acbabc4e9c7a86abe40a47380a5eab06b4e0ae0fdbce1d06949b1`.
Mac framework SHA256:
`4a5b69c0cfc84f2de6492077310d61372bb26c5981f7f772c87b833c57d7509a`.
Full runtime regression and a fresh Linux archive must be evaluated against
this newer patch; previous `37612ae` network evidence is not proof for it.
