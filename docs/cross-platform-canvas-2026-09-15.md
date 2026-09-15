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
this candidate is described below; the source hypothesis alone is not acceptance.

## Exact-rounding candidate results

Linux candidate `808a1551b6b7d103d36f786c74ec430fd378065e83e7946415bde5f4b66cf8f2`
built successfully (57 incremental steps, 4m23s), then was staged and extracted
as a ClawBrowser runtime with its own AppArmor user-namespace profile. The
archive SHA256 is `62cbdf9cf25d466678c2daa3697039533b57adfd5be33f2808c3fa1048fb6086`.

Compared with the same saved macOS ARM64 observations:

- All **32/32 font-free primitive observations match**, previously 28/32.
- All **48/48 color-format observations match**, previously 24/48.
- All **12/12 PNG observations still match**.
- The text/shape corpus still differs in **60/60** comparable observations.

The protected test run passes 4 tests in 36.59s; the separate Linux two-font
control was skipped because that targeted invocation did not supply its two
font configs. Full regression with those configs is a separate run. These
results validate the isolated coverage correction, not universal Canvas
independence or a green PixelScan verdict.

An opt-in software-raster microbenchmark (`CLAWBROWSER_QA_RASTER_TIMING=1`)
runs 10 warmups and seven batches of 30 gradient/ellipse/readback operations.
Sequential before/after runs on the same Linux host recorded median milliseconds:
native 0.420 -> 0.437; protected 0.673 -> 0.690. Samples overlap. This small
single-run comparison is **not** a statistically established performance
regression or a general performance acceptance gate. Both runs passed their
readback checks (2.62s / 2.59s). No build was running during those timings.

`test_canvas_corpus_stages.py` adds cumulative gradient, transformed curve,
Latin, fallback text and shadow snapshots to locate the remaining divergence,
without removing any assertion from the full corpus.

The staged recipe passes locally on both hosts but differs across hosts:
the gradient and transformed curve match; the **first differing snapshot is
Latin/Cyrillic text in Arimo**. The 40-case follow-up using both `auto` and
`geometricPrecision` still differs in 24 text-and-later snapshots, in both
backing formats. Mac 12.06s, Linux 24.14s (Linux full regression was also
running; these durations are not performance comparisons). This does not
attribute the later fallback/shadow differences independently because the
snapshots are cumulative. Next investigate face selection and SkFont render
settings, not another speculative global Canvas change.

Full headful Linux regression on `808a155` completed **121 passed, 18 skipped,
zero failures in 266.93s**, including both Linux host-font controls. The two
opt-in performance cases account for two added skips; the new primitive test
accounts for one added pass versus the previous full run. The staged recipe
was added after collection and was tested separately as described above.
The full run does not replace the separately configured real TURN/TLS and
packet-capture tests. Mac rebuild remains blocked by unaccepted Xcode license;
the existing Mac candidate was used for comparison, not a new Mac build.

## Text metrics attribution

The bundled Arimo-Regular.ttf bytes match on both hosts (SHA256
`eafef8c99e94d10f17506c125e24d98a84256e0e665e6c659498eca96b19e148`).
The staged diagnostic now records `text_metrics` in addition to pixel hashes.
For `17px Arimo`, `Latin 0123 Привет`, macOS width is 140.814453125; Linux
default is 138. With `geometricPrecision`, Linux width becomes 140.814453125,
but glyph bounds still differ. Reports: `canvas-corpus-metrics.xml` on Mac and
`coverage-rounding-metrics.xml` on Linux; per-host assertions passed in
12.17s / 24.18s.

There are two distinct source leads: Linux `CreateSkFont` consumes system
`WebFontRenderStyle`, whereas Mac sets subpixel and linear metrics directly;
`skia_text_metrics.cc` also explicitly uses path bounds on Apple versus
integer glyph bounds elsewhere. Matching only the advance width therefore
cannot establish identical rasterization or all metric surfaces.

Patch 042 is a Linux QA candidate aligning protected Fontations strike defaults
with the existing managed Mac path, while preserving native policy. This is
not a complete cross-platform font fix. It does not
change metric-bound helpers, Windows behavior, Canvas noise, or any website.

The candidate built in 4m39s (47 steps), binary SHA256
`eefc94ed2b0eb5dad04281bf26d306b8c73ce092d02310dd6043b21040d6d7ee`,
archive SHA256 `8a879e932150567412e4e49b21505bc8e1a0d6e907403ecc003822d65d783cef`.
It was extracted separately with its own AppArmor user-namespace rule.

The new linear-advance assertion fails on the previous Linux candidate
(`font-advance-old-negative.xml`, 25.26s) and passes on Mac (12.50s). The new
Linux candidate passes that assertion and both native/closed-catalog policy
tests: **3 passed / 26.21s**. The recorded native-policy metrics are identical
before and after. Closed-catalog Latin width is now 140.814453125 in both
rendering modes, matching Mac. The pixel comparison still differs in 24/40
staged observations, so advance normalization alone is insufficient.

Full headful Linux regression completed **124 passed, 18 skipped, zero failures
in 291.95s**, with both host-font configs. Next: policy-scoped path-bound
normalization in `skia_text_metrics.cc`, while preserving native glyph metrics,
then distinguish remaining coverage/position differences from font selection.

Opt-in `CLAWBROWSER_QA_FONT_PIXELS=1` exports compressed synthetic Latin
control pixels, not page/user data. `scripts/compare_font_pixel_controls.py`
validates matching dimensions/seeds and quantifies differences (exit 1 when
different). The unchanged-control comparison returns zero. Comparing Mac to
Linux `eefc94e` finds 727 differing pixels in `auto`, 771 in
`geometricPrecision`, max channel delta 20, with no alpha differences. All
differences lie inside the text region (x=4..143, y=31..47).

A separate **diagnostic-only** `CLAWBROWSER_QA_TEXT_GAMMA_CONTROL=1` supplies
the existing `--text-contrast=0 --text-gamma=0` switches. Skia's Linux/Mac build
defaults differ. **Correction after auditing the surrounding preprocessor
guard:** Chromium forwards these switches to the renderer only on Windows.
The Linux experiment therefore did not establish that renderer gamma changed;
its unchanged pixels cannot rule out gamma as a cause. It is not a product fix
or acceptance run. Mac pixel export passed in 12.07s; ordinary Linux export 24.30s; gamma
control 24.23s. The comparison output labels whether either side used the
gamma control. No global gamma defaults were changed.

## Managed outline bounds

Patch 043 uses the existing precise outline-bound helper for managed font
policy in HarfBuzz extents and single/batched glyph-bound lookup. It preserves
bitmap-only fallback, outward rounding for non-subpixel strike bounds, and
the original native-policy branches. The regression pins the known Arimo
outline bounds, not only its advance width. Previous Linux `eefc94e` fails
four of five expected fields; native passes (2.33s). Mac passes both policy
cases (4.40s).

New Linux binary `45004e1e3e11f794920e03ac6d21cfce936c3c269fe831575c62c9cb2607050e`
built in 4m30s/47 steps. Archive SHA256:
`cfa26b59e4735e1d633d9b79102fcec56a9f13c5b85ad2d8828da27e23ac6022`.
After separate extraction and AppArmor setup, the targeted run passes
**3 tests / 26.17s**. Native-policy metrics match the previous binary exactly.
Managed Arimo now matches the Mac reference for width, left/right bounds,
ascent and descent in this fixture.

Text pixel differences are unchanged (727/771), establishing that fixing
these metric surfaces is separate from the remaining raster mismatch.
Full Linux regression passed **124 tests, 18 skipped, zero failures in 288.59s**.
This does not fix the separate
`fontBoundingBoxAscent/Descent` host-specific vertical-metric adjustments in
`FontMetrics::AscentDescentWithHacks`, nor prove all scripts or platforms equal.

## Coverage and text-transfer isolation

The source-over control (`CLAWBROWSER_QA_TEXT_SOURCE_OVER=1`) still differs
across hosts: 628/666 Latin pixels, maximum channel delta 18. Both hosts use
the same changed recipe; it is not compared to the default multiply recipe.
Mac passes in 11.96s, Linux in 24.15s. The comparator now rejects mismatched
blend-control metadata.

`test_font_mask_controls.py` draws black text on white, independently varying
integer/fractional x and y and text rendering mode. All eight cases still
differ (640/656/680/683 pixels depending on settings; max delta 68; alpha
unchanged). Mac passes in 3.22s, Linux in 2.52s. Across these controls, all
5,318 differing red-channel samples are darker on Linux, with none darker
on Mac (comparing values after dropping the noise LSB). This is evidence of
a systematic coverage/transfer difference, not proof of its exact cause.

Patch 044 is a new QA candidate setting explicit surface properties on both
protected Canvas2D providers: unknown LCD geometry, zero contrast, sRGB text
gamma (Skia gamma 0). It keeps native and non-2D surface properties unchanged.
Opaque protected canvases also use grayscale coverage to avoid host LCD
geometry. Unlike the earlier ineffective CLI experiment, this change acts
directly where the renderer constructs raster surfaces. Build/runtime
validation is pending; no global Skia gamma defaults or Canvas noise change.

The candidate built successfully (47 steps / 4m34s). Binary SHA256:
`814e11ae6d5dabc9fe47c37894c999a32b0b2208f93b48414f6796260d6f3fbd`.
Archive SHA256: `aa5e9190a8af41203daa395bd0cdcfa864334178519cdbe1fe18858e4790b413`.
It was separately extracted with a scoped AppArmor rule and the sandbox intact.

All **eight protected black-on-white controls now exactly match the pinned
Mac hashes** (`font_mask_reference.json` records recipe, seed, font and binary
provenance). The previous Linux binary fails the new assertion in 2.72s; Mac
passes both policies in 5.86s; new Linux protected/native runs pass in 2.46s /
2.44s. The eight native-policy pixel buffers are byte-for-byte unchanged from
the previous Linux candidate. This confirms the isolated text-transfer cause,
not all possible text drawings or all platforms. Full regression and the
larger cross-host corpus comparison are running separately.

The new full headful run completed **126 passed, 18 skipped, zero failures in
295.88s**. A separate 12-way test covers alpha true/false, both readback hints,
and DOM/Offscreen/Worker: all match the same pinned protected reference
(3.01s). The previous binary fails (3.08s) and produces two distinct hashes
instead of one across those surface settings. This extra test was added after
full-suite collection and is not counted in the 126.

The larger comparison remains incomplete: PNG 12/12 and color-format 48/48
still match Mac. In the 40 cumulative text/shape stages, differences fall
from 24 to **12**, now **only unorm8 backing**; all float16 stages match,
including fallback text and the shadow. The five-seed unorm8 corpus still
differs in 60/60 observations. Next isolate the remaining low-precision blend
and coverage arithmetic; do not treat the small mask reference as universal
Canvas independence. PixelScan has not yet been rerun on this candidate.

## Uniform low-precision blend rounding

Patch 045 replaces non-NEON Skia `div255`'s approximate `(v+255)/256`
with the exact integer formula already used on ARM. This is a general
software-raster change, including native Canvas: performance and regression
checks are necessary, not optional. It leaves the NEON implementation intact
and does not change fingerprint noise or special-case a site or drawing.

Linux binary SHA256:
`b287f4bceec2c7bb2618ddb1952cd2484c9bd5d571d7c731ba3852057a714cca`.
Archive SHA256:
`4528e598aae425936a1a424fd01c85130e8d295d137f1aa44cc7fb57cada1c98`.
Build: 57 steps / 4m27s; separately extracted with sandbox enabled.
The targeted run passes five tests in 34.39s. All **40 cumulative Canvas
stages now exactly match** the saved Mac ARM64 observations (previously
12 differed). This is bounded fixture evidence, not universal rendering
equivalence. The 40 default-recipe observations are pinned in
`canvas_stages_reference.json`; the existing Mac ClawBrowser passes the new
assertion in 11.94s. Alternate diagnostic recipes are excluded from that
specific golden comparison. The new Linux golden assertion subsequently
passes too (three tests including two vertical-metric observations / 26.08s).

**This candidate is not ready:** full headful regression reports **123 passed,
4 failed, 18 skipped / 300.11s**. All four failures are the existing native
and protected cross-context corpus gates, with and without the two Linux
fontconfig controls. DOM/Offscreen/Worker and readback hints agree within each
launch. The difference is between ordinary and `--disable-skia-runtime-opts`
launches, for recipe seeds 7 and 65537 only. The ordinary runtime now matches
the saved Mac corpus completely; the comparison across both launch modes is
108/120 equal, with 12 differing fallback-CPU observations. Do not weaken
that gate or promote the candidate based only on the ordinary runtime.

Sequential ABBA raster timing used 7 samples of 300 iterations per mode,
after build/staging/full tests finished. Per-run medians in milliseconds:

| Run | Binary | Native | Protected |
| --- | --- | ---: | ---: |
| A1 | 044 | 0.4863 | 0.7180 |
| B1 | 045 | 0.4333 | 0.7833 |
| B2 | 045 | 0.4750 | 0.7247 |
| A2 | 044 | 0.4350 | 0.7120 |

The small benchmark has overlapping sample ranges and cannot establish
absence of regression; protected timings may be modestly slower. In any
case, the CPU-path correctness failures block acceptance independently.
`test_canvas_recipe_steps.py` is an opt-in diagnostic using the same shared
recipe as the original corpus, recording per-draw deltas without weakening
the existing comparisons.

## Baseline CPU fused arithmetic candidate

The per-draw diagnostic localized the first differences to seed 7/draw 11
(two color channels) and seed 65537/draw 1 (one channel), maximum delta 2,
no alpha differences. Patch 046 gives baseline x86 `mad`/`nmad` the same
single-rounding semantics as ARM64/AVX2 using `std::fma`, without requiring
hardware FMA instructions on older CPUs. This also affects the native
software path; baseline-CPU performance must be measured separately.

Binary `a2f2642c02cf75f6e0be59ac53a607d1d45b027ad99ec4fb781b3f6c138b7a6c`
built in 57 steps / 4m26.69s. Archive:
`9bc7bf48162b7e9308b317f070490d6a756ac44716191c8c5513fe02a5eb151e`.
The new per-draw test passes (29.72s), with zero changed channels at every
captured draw. Both original native-host corpus gates now pass (46.16s).
All **60 protected corpus observations match macOS**, including the disabled
runtime-optimization path that failed on 045. The shared recipe extraction
also passes on the unchanged Mac binary (17.11s).

These are targeted results, not a full-suite/network/PixelScan acceptance
for this new artifact. The separate vertical font-metric control still
finds 56/84 mismatches before a fix; its new assertion fails the old Linux
candidate while native policy passes (2.49s). Mac passes both cases (3.75s).

The forced-baseline ABBA timing exposes a substantial performance cost:
native medians 0.7717/0.8163ms before versus 2.2697/2.3467ms after;
protected 1.1133/1.0777ms before versus 2.5963/2.5643ms after. Seven samples
of 300 iterations per run, after other QA workloads finished. This is
roughly 2–3x slower on that path, not an acceptable "no regression" result.
An optimization preserving correctly rounded FMA is still under investigation;
do not claim performance readiness from the correctness improvement.

## New artifact network acceptance

Linux `a6f3c1a` passes controlled IPv4/IPv6 STUN packet capture: independent
positive control 4 packets, browser 0 packets through HTTP/SOCKS5, zero kernel
drops. Real TURN/TLS echo passes both proxy schemes, 2 tests / 18.05s, with
successful selected relay pairs and positive sent/received bytes. Both bounded
test services were stopped and verified inactive. These results do not turn
PixelScan's still-negative fonts/canvas classification into a pass.
