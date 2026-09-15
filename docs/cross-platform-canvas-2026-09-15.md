# Cross-platform protected raster comparison

Compared saved headful observations from macOS ARM64 staged launcher `945d653`
and Linux x64 extracted runtime `a6f3c1a`. These are ClawBrowser artifacts,
not stock Chromium controls. They differ in the navigation Client Hints fix;
no Canvas implementation changed between those candidates. This is still a
two-host observation, not proof for all operating systems or processors.

The protected color-format matrix has 24 matching and 24 differing hashes.
All differing cases use an `unorm8` Canvas backing; `float16` backing cases
match in both readback formats. The PNG matrix matches 12/12. The five-seed
text/shape corpus differs in all 60 comparable observations. Within each host,
the existing protected context/hint consistency assertions pass.

`scripts/compare_canvas_observations.py` compares JUnit properties and exits
nonzero for cross-report differences; it does not weaken existing assertions.
Its initial comparison reports 84 differences among 120 comparable observations.

## Smaller font-free reproduction

Added `test_canvas_raster_primitives.py` with a fixed fixture seed
`1234567890`, opaque background, both backing formats, and both readback hints.
The first 16 cases (solid, ordinary gradient, ellipse, screen composite) match
between hosts. Adding a Display-P3 gradient alone still matches. Combining
that gradient with a translucent ellipse under the screen blend operation
differs only with `unorm8` backing: **2 differing cases out of 24**.
The same combination with `float16` backing matches.

Both per-host tests pass their unchanged within-host hint, alpha and repeated
readback assertions: macOS 3.32s, Linux 2.68s. Cross-report comparison exits 1,
as it should. Reports: `raster-primitives-p3.xml` in each host's evidence set.
This isolates an architecture/platform-sensitive compositing path without
fonts, worker differences, or a PixelScan-specific drawing. It does not yet
identify the faulty instruction or prove that this causes PixelScan's verdict.

Next: inspect and isolate the low-precision software blend implementation,
including rounding and format conversions. Do not globally replace backing
formats, disable noise, or claim the issue solved solely from this diagnostic.

Source lead: Skia's `SkRasterPipeline_opts.h` explicitly implements `div255`
with accurate rounding on NEON but an approximation on other architectures.
The shared low-precision `lerp` uses that helper. The default `screen` blend
itself uses `div255_accurate`, so blaming that blend instruction alone would
be premature; edge coverage interpolation is a candidate to isolate next.
This is source evidence of differing arithmetic, not yet runtime attribution.

The follow-up 32-case control adds integer-aligned and fractional rectangles
over the same gradient. Only the fractional rectangle and ellipse differ on
`unorm8` (four observations, two readback hints each); aligned rectangles match.
This further implicates partial edge coverage rather than the interior blend.
Per-host tests pass (macOS 3.61s, Linux 3.09s); cross-host equality still fails.

Patch 041 is a QA candidate replacing low-precision `lerp`'s `div255` with
`div255_accurate`. NEON already used exact rounding; x86 changes to the same
formula. This is a general Skia coverage correction, not a site or color
exception. **It also affects native software rasterization**, so native
conformance and performance must be checked; it is not a policy-scoped change.
Canvas noise and format selection remain unchanged. Runtime validation of
this candidate is pending; the source hypothesis alone is not acceptance.

## New artifact network acceptance

Linux `a6f3c1a` passes controlled IPv4/IPv6 STUN packet capture: independent
positive control 4 packets, browser 0 packets through HTTP/SOCKS5, zero kernel
drops. Real TURN/TLS echo passes both proxy schemes, 2 tests / 18.05s, with
successful selected relay pairs and positive sent/received bytes. Both bounded
test services were stopped and verified inactive. These results do not turn
PixelScan's still-negative fonts/canvas classification into a pass.
