# Cross-platform protected raster comparison

## Current acceptance boundary

This report preserves the investigation chronologically; initial failures below
are not the latest result. Linux `c067039` completes the headful run with
134 passed and 21 skipped, matches all 120 common Mac corpus/format hashes, 32
primitives, 16 SVG observations and 30 bitmap observations, and passes the
controlled STUN/TURN packet tests. Exact artifact
hashes, timings and caveats appear in the later sections.

**Not ready for merge/release:** forced-baseline CPU cost remains substantial,
PixelScan's font/Canvas classification remains negative, and the latest changes
still need Mac/Windows binary acceptance. Do not confuse reference Mac results
with a rebuild of these latest patches, or site diagnostics with a green scan.
The additional SVG/PNG mismatch found outside the earlier corpus is fixed in
this latest Linux artifact; see patch 049's rebuilt-browser result below.

**New coverage failure:** the subsequent 26-operation translucent blend matrix
fails on this same Linux artifact, although the earlier 198 observations still
match. In particular, baseline x86 float16 rendering differs from normal CPU
dispatch. See the final section; do not extend the earlier corpus result to
all blends or half-float surfaces.

## Latest managed-site check (patch 049)

The extracted `c067039` Linux executable was also verified through the managed
QA profile: 35/35 internal identity checks pass. The executable path and SHA256
were checked independently of launch flags. This is ClawBrowser, not a stock
Chromium control. Managed Remote Control authentication still returns HTTP 401;
the already-running browser's successful checks do not certify Remote Control
or Desktop OAuth.

The ordinary PixelScan fingerprint page still reports **inconsistent / Masking
detected**, with **No proxy detected** and **No automated behavior detected**.
The original screenshot was retained before a separate instrumented diagnostic.
That diagnostic reads the existing classifier booleans without replacing its
inputs or result: fonts=false and canvas=false; the other 11 observed predicates
are true. Cross-platform raster equality therefore does **not** by itself resolve
the site's classification of enabled pixel protection.

The ordinary WebRTC checker still displays **WebRTC leak detected**. Passive
observation of its successful HTTP 200 exchange found all 12 candidate arrays
empty; the HTTP IP matches the fingerprint page and is not the QA server IP.
This repeats the [empty-candidate diagnosis](pixelscan-webrtc-empty-candidates-2026-09-15.md)
on the latest artifact, not a green site result or a new demonstrated host leak.
No site-specific exception or classifier modification was used.

Local evidence set `clawbrowser-linux-c067039` contains
`pixelscan-exact-srcover-049.{json,png}` and
`webrtc-exact-srcover-049.{json,png}`. Screenshots were visually inspected.
Raw screenshots contain the proxy egress address and are retained locally,
not committed to this public report. The sanitized WebRTC counts above contain
no addresses, SDP, credentials, or authentication material.

## Initial cross-host failure

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

## Closed-catalog vertical metrics

Patch 047 skips host-specific VDMX, Linux descent-borrowing and macOS family
adjustments only for managed closed-catalog fonts. Native policy and explicit
ascent/descent overrides retain their existing paths. The regression records
six families, seven sizes and both text-rendering modes, separately from
painted glyph bounds.

Binary `003ab5f28fa54f9fc1e787e90ad6fe96ac533f2ebceb8570202ab37c55749f9e`
built in 47 steps / 4m25.26s. Archive:
`2f6a793ef786690578b2a5b77fff1a81a35972d0d089cfca5e7b219f0641c9a4`.
After extraction with the sandbox enabled, five targeted tests pass in
28.76s. All **84 managed vertical-metric cases match the Mac reference**;
the 84 native-policy rows are exactly unchanged from 045. The existing
outline/advance and 40-stage Canvas golden checks also pass. This artifact
has not yet completed a new full-suite or network acceptance run.

## Exact arithmetic optimization — still in QA

Patch 048 introduces a separately testable math header. A binary32 product
is exact in binary64; conversion of the binary64 sum can double-round only
at a binary32 midpoint. The scalar helper recovers the addition residual
with TwoSum at those midpoints and preserves signed zero; exceptional ranges
retain `std::fma`. This relies on Skia's round-to-nearest FP environment.
The x86 implementation evaluates four lanes with SSE2 double operations and
uses scalar correction only for lanes needing it. No hardware FMA is required.

The actual header passes **10,039,200 scalar plus 10,039,200 mixed SIMD-lane
comparisons** against `std::fma`, with baseline SSE2 compiler flags disabling
AVX/FMA. Inputs include exceptional values, randomized bit patterns and
adversarial midpoint cases. The naive-double negative control fails **3,217**
comparisons, ensuring the test exercises double rounding. The regression is
wired into CI via `scripts/skia_fma_rounding_test.sh`, which extracts the
actual header from the patch rather than testing a copied implementation.

The first two scalar optimization candidates retained correct browser
results but did **not** improve baseline performance. Do not present those
as a successful speedup: candidate `e636472` took about 2.33–2.37ms native /
2.72–2.73ms protected, and `db46d7e` about 2.52–2.62ms / 2.81–2.95ms on
the forced-baseline workload. Ordinary runtime measurements overlapped the
previous candidate. The vectorized revision's measured results follow.

The vectorized candidate is now built: ELF
`5f7fb6e5686c78c775f58f2fdbbdc8202e5b67b5f46cf5b17e8bd2b04d393bb8`,
archive `6393c8e5f6dcd1177fbe663de248fc68d391dd693bcd89f5077db329c7f9a7ba`.
Four targeted tests pass in 49.43s (per-draw CPU comparison, native/managed
vertical metrics, 40 Canvas stage goldens). CI passes both contract jobs.
The standalone ABBA benchmark uses 045 / new / new / 045, seven samples of
300 iterations, with no concurrent QA workloads:

| CPU path / policy | 045 medians, ms | New medians, ms |
| --- | --- | --- |
| Default / native | 0.4763 / 0.4457 | 0.4540 / 0.4290 |
| Default / protected | 0.7280 / 0.8210 | 0.7733 / 0.7207 |
| Forced baseline / native | 0.8340 / 0.7813 | 1.8937 / 1.9203 |
| Forced baseline / protected | 1.0763 / 1.0797 | 2.3337 / 2.4227 |

Vectorization improves on the scalar candidates but still leaves a roughly
2.2–2.5x forced-baseline regression. Default-path timings overlap. These
measurements do not justify declaring performance ready; the baseline cost
remains an explicit release risk. Full-suite and network tests on this exact
artifact are reported below.

The build helpers now support resetting explicitly patch-created Skia files,
which are absent from upstream nested HEAD. They continue rejecting unexpected
absent targets and preserve unrelated files. The new regression fails the
old helper and passes both updated build paths. No full-checkout reset was
run on the live QA source tree during a build.

## New artifact network acceptance

### Hot-path inlining follow-up

Disassembly of `5f7fb6e` showed an out-of-line `ClawbrowserFma4` with stack
setup in the ordinary path. The follow-up forces the fast vector calculation
inline and outlines only exceptional lane repair. Arithmetic is unchanged.
The actual header again passes 10,039,200 scalar and 10,039,200 SIMD comparisons.
An SSE2 microbenchmark is retained in `clawbrowser/test/benchmarks` for reproducibility;
its roughly 10% improvement is not browser acceptance.

New ELF `5afeb33bd2460710476abe9501ef908e5ddf18aab86cf791afe78cb6f3beb278`
built in 57 steps / 4m50.49s. Archive:
`95fefb1eb354de8aefc8a6973b1b08c88655a21ffe19921e33839e29fa6ea445`.
The executable grows by 205,416 bytes. Only the slow helper remains as an
out-of-line symbol. Four targeted browser tests pass in 47.40s.

ABBA before/after timings, seven samples of 300 iterations, show partial
forced-baseline improvement: native 2.0370/2.0870ms to 1.8550/1.9410ms;
protected 2.3277/2.4563ms to 2.1413/2.1063ms. The initial ordinary-path
measurements were slightly worse, so a longer ordinary-path control followed:
six interleaved runs, seven samples of 1,000 iterations each.

| Ordinary path | Before: three run medians, ms | After: three run medians, ms |
| --- | --- | --- |
| Native | 0.4430 / 0.4832 / 0.4586 | 0.4847 / 0.4918 / 0.4558 |
| Protected | 0.7439 / 0.7578 / 0.7601 | 0.7790 / 0.7548 / 0.7473 |

These ranges overlap; they do not establish universal absence of a regression.
Forced-baseline cost versus the original non-fused implementation remains
substantial and open. The new full suite completes with **131 passed, 19 skipped
in 318.00s**. All 120 common PNG/color/corpus observations and 32 primitives
still match the Mac reference. Separately, IPv4/IPv6 STUN records 4 positive
control packets and 0 browser packets; real TURN/TLS passes both proxy schemes
in 19.884s. Direct-TCP capture records 7 positive-control packets, 0 browser
direct packets and 280 proxy-origin packets. Both captures have zero drops,
TLS trust verification stays enabled, and both bounded services are inactive
afterwards. Benchmark timing finished before these functional/network runs.
The committed microbenchmark also compiles against the actual header and
passes its one-iteration smoke check. CI passes both contract jobs.

This is partial performance progress, not full release readiness. Managed-site
screenshots below remain observations of their explicitly identified earlier
artifact; no new green PixelScan result is claimed for this arithmetic revision.

### Four-lane classification follow-up

The next revision packs the high/low words of both double-vector sums and
classifies four lanes together with SSE2 integer operations. TwoSum residual
correction remains required at midpoints; subnormal/non-finite cases retain the
scalar fallback. Eight explicit signed overflow/subnormal boundary cases were
added. The actual header passes **10,039,208 scalar plus 10,039,208 SIMD
comparisons**. The naive-double negative control now fails 3,223 comparisons,
including six new boundary failures. No rounding semantics were relaxed.

ELF `65904c2d31c5795fff7c56f2af416077ed4ac82ad7afd3ef6a666b1fa7d9d8dd`
built in 57 steps / 4m56.72s. Archive:
`46810031ce5a96bff45c5a0242df37319b8a7a8cb742b9e9063a5b1656ba59d0`.
Four targeted browser tests pass in 49.02s. No other QA CPU work ran during
the subsequent timing comparison:

| CPU / policy | Previous medians, ms | Packed-mask medians, ms |
| --- | --- | --- |
| Default / native | 0.4902 / 0.4609 / 0.4809 | 0.4842 / 0.4816 / 0.4748 |
| Default / protected | 0.8377 / 0.8031 / 0.7658 | 0.7823 / 0.8417 / 0.7989 |
| Forced baseline / native | 1.8493 / 1.9213 | 1.6363 / 1.6460 |
| Forced baseline / protected | 2.4777 / 2.2747 | 1.9553 / 1.9937 |

Default-mode runs are interleaved before/after/after/before/after/before,
seven samples of 1,000 iterations. Baseline uses ABBA and 300 iterations per
sample. Run-mean improvement on the baseline workload is about 13% native and
17% protected, while default-mode ranges overlap. This is another partial
improvement, not elimination of the cost relative to non-fused arithmetic.
The sandboxed extracted artifact completes the full headful suite with both
host Fontconfig variants: **131 passed, 19 skipped in 335.61s**. Skips are not
passes. All 120 common Mac corpus/format observations and 32 raster primitives
match reference binary `945d653`, with zero differing hashes. This does not
substitute for a latest-patch Mac or Windows rebuild.

Separate controlled network acceptance on this same artifact also passes:

- IPv4/IPv6 STUN: 4 independent positive-control packets, 0 browser packets
  across HTTP/SOCKS5, and 0 capture drops.
- TURN/TLS: 2 echo tests pass in 19.688s through HTTP and SOCKS5, with relay-only
  selected candidates/pairs and positive sent/received bytes. The independent
  direct-TCP control records 7 packets; browser direct TCP records 0 and
  proxy-origin traffic records 302, with 0 capture drops. TLS trust is verified.
- Both bounded network services were verified inactive after testing.

Evidence files are `fast-fma-048-v5-full.xml`,
`fast-fma-048-v5-stun-pcap.json`, `fast-fma-048-v5-turn-route.xml` and
`fast-fma-048-v5-turn-route.json`. The retained managed-site screenshots belong
to earlier artifact `5f7fb6e`; no green PixelScan result is claimed for this
candidate. Overall baseline cost and platform/site acceptance remain open.

### Numerical portability and evidence validation

CI commit `a5685b1` adds independent host-compiler checks of the actual header
extracted from patch 048, without a Chromium checkout or credentials. Run
[35008472929](https://github.com/clawbrowser/clawbrowser/actions/runs/35008472929)
passes on Linux GCC, Linux Clang 18, Windows x64 Clang 20 with the MSVC library,
and macOS ARM64 Apple Clang 21. Each x64 job checks 10,039,208 scalar plus
10,039,208 SSE2 results; ARM64 checks 10,039,208 scalar results, with no SSE2
path. Every negative control detects 3,223 naive-double mismatches. The x64
harness now fails compilation if the SSE2 implementation is accidentally absent.
This is numerical portability evidence, **not Windows/macOS browser acceptance**.

The cross-platform comparator now rejects failed/errored protected test cases,
ignores skipped cases and rejects multiple distinct hashes for one observation.
Previously two identically non-deterministic sets could compare equal. Six
unit regressions exercise these failure modes and exit codes. Rechecking the
saved latest Linux/Mac reports with these stricter rules still yields all 120
common corpus/format matches plus 32 primitive matches and zero differences.

### SVG-image text follow-up: cross-host difference remains

`test_svg_font_canvas.py` covers text inside an SVG image drawn into DOM and
Offscreen Canvas, rather than ordinary Canvas `fillText`. It contains serif,
sans-serif, monospace, Arimo and a missing-family fallback, with Latin,
Cyrillic, Arabic, CJK and emoji. A text-free copy is a negative control:
more than 500 channel values must differ, preventing a background-only image
from satisfying the test.

Linux `65904c2` passes internal equality across two host Fontconfig settings,
normal/disabled Skia CPU options, and both Canvas kinds (1 test / 9.61s).
Reference Mac `945d653` passes its two CPU modes and both Canvas kinds
(1 test / 5.12s). **All four comparable cross-host hashes differ.**
Row-level diagnostics localize differences to rows 38–40, 43–44, 53 and 55–57,
inside the second text line (`العربية 漢字 🧭`). Other rows, including the
gradient and the other four text lines, match. Text-free control differences
are 11,897 channel values on Mac and 11,898 on Linux.

This narrows the problem to multilingual/emoji SVG rendering in this sample;
it does not yet identify the exact glyph, prove host font enumeration, or
separate missing Mac updates from a remaining platform-dependent path.
Do not call the passing internal tests cross-platform acceptance. Evidence:
`fast-fma-048-v5-svg-font-rows.xml` and `mac-reference-svg-font-rows.xml`.
The next diagnostic should split Arabic/CJK/emoji, retaining the same renderer
and protected policy. No browser patch or checker-specific exception was added.

That split is now executable in the same test. Linux passes internally in
22.72s and the Mac reference in 10.04s. Of 16 comparable observations, the
eight Arabic-only/CJK-only observations match; all eight mixed/emoji-only
observations differ. This isolates the sample's mismatch to color emoji, not
Arabic or CJK text. Saved reports end in `svg-font-scripts.xml`. The Fontations
bitmap-glyph path scales a decoded image with linear filtering into a temporary
bitmap; testing that general image-sampling path is a candidate next diagnostic,
not yet a demonstrated cause. No rendered-emoji replacement was introduced.

### Fractional bitmap follow-up and source-over candidate

The new `test_canvas_image_sampling.py` uses a seeded RGBA bitmap, fractional
placement/scaling and all three image-smoothing qualities, without fonts.
Both hosts pass internal DOM/Offscreen and CPU-option equality. Protected-mode
cross-host comparison finds 12 differences in 30 observations: all scaled PNG
paths differ; source pixels, unity-size copies and scaled Canvas-source paths
match. Native-policy diagnostic controls differ on both scaled source types.
This is a test-only control, not a change to the product protection policy.
Synthetic-pixel diagnostics find 3,796 differing channels in the protected
low-quality PNG sample (Linux minus Mac: 2,830 at -2 and 966 at +2); the
Canvas-source sample has zero differences.

Source inspection finds an unnormalized legacy bitmap blitter:
`SkBlitRow_opts.h` uses approximate `/256` source-over on SSE2/AVX2, while NEON
uses rounded `/255`. Patch 049 makes the x86 helpers and scalar tail use the
same rounded arithmetic, retaining vectorization, saturation and no
pixel-dependent branches. It does not alter the unrelated constant-color
blitter. This is a candidate explanation pending a rebuilt browser test.

The actual added header passes 17,777,216 scalar and SSE2 comparisons, and a
separate AVX2 run passes the same number for all three paths. The exhaustive
byte-channel/alpha matrix plus packed random-lane cases includes saturation.
An approximate `/256` negative control differs 3,604,033 times. The tests are
added to the Linux/macOS/Windows numerical CI matrix. Browser build and
cross-host acceptance of this patch are still pending.

The first Chromium compile caught a portability detail absent from the original
standalone test: Skia selects its AVX2 translation unit with target pragmas,
which do not define `__AVX2__`. The helper now also recognizes Skia's target
level and is forced inline to avoid cross-target helper symbol selection.
A baseline-compiled Clang test using the same target-pragma mechanism passes
all 17,777,216 scalar/SSE2/AVX2 comparisons. Linux Clang CI runs that case when
the runner advertises AVX2; a browser rebuild is still required.

### Patch 049 browser result

The corrected build completes 57 steps in 306.31s. ELF:
`c0670393e68f9df237c9b552483abbc61397608dcc98a86ff057a3cc520ce173`.
Extracted archive:
`cab51caad0fe9b5a3a70f5c850c384dbdc79dd702d62a4b68c4d6101618a6cc7`.
The scoped user-namespace policy is loaded and the browser sandbox remains on.

Three targeted tests pass in 56.82s. Against the unchanged Mac reference,
**all 16 SVG observations and all 30 protected bitmap observations match**.
The previously differing 8 emoji/mixed observations and 12 scaled-PNG
observations are now equal. This confirms the source-over fix for these
protected examples. Native-policy scaled-image observations still differ
across hosts; native mode is not certified as normalized rendering.

An isolated bitmap benchmark uses a seeded transparent PNG, fractional scaling,
readback, seven samples of 1,000 iterations and old/new/new/old runs:

| CPU / policy | Previous run medians, ms | Patched run medians, ms |
| --- | --- | --- |
| Default / native | 0.1593 / 0.1661 | 0.1645 / 0.1562 |
| Default / protected | 0.5955 / 0.5330 | 0.5514 / 0.6017 |
| Baseline / native | 0.3555 / 0.3438 | 0.3446 / 0.3536 |
| Baseline / protected | 0.7289 / 0.7868 | 0.7211 / 0.7439 |

Ranges substantially overlap; this workload does not demonstrate a meaningful
performance regression or a universal speedup. All 16 benchmark cases pass.
No build, managed site session or other QA CPU workload ran during timing.
The full sandboxed headful suite with both host Fontconfig settings completes:
**134 passed, 21 skipped in 389.20s**. The 21 skips include two newly added
opt-in bitmap timing cases (run separately above), earlier opt-in network/API
cases and platform-specific cases; they are not passes. Strict comparison of
the full report matches all 120 common Mac corpus/format observations, 32
primitives, 16 SVG observations and 30 protected bitmap observations, with zero
differences. The Mac reference remains `945d653`, not a latest-patch rebuild.

Separate real-network acceptance of this same artifact:

- IPv4/IPv6 STUN: positive control 4 packets, browser 0 across HTTP/SOCKS5,
  capture drops 0.
- TURN/TLS: 2 tests pass in 16.549s, working relay echo through both proxy
  schemes, positive sent/received bytes and verified TLS trust. Positive direct
  TCP control 7 packets, browser direct 0, proxy-origin 252, capture drops 0.
- Both bounded network services were verified inactive after testing.

Evidence: `exact-srcover-049-full.xml`, `exact-srcover-049-stun-pcap.json`,
`exact-srcover-049-turn-route.xml` and `exact-srcover-049-turn-route.json`.
No current-artifact PixelScan pass, latest Mac/Windows browser acceptance,
complete desktop OAuth E2E or overall merge/release readiness is claimed.

Linux `a6f3c1a` passes controlled IPv4/IPv6 STUN packet capture: independent
positive control 4 packets, browser 0 packets through HTTP/SOCKS5, zero kernel
drops. Real TURN/TLS echo passes both proxy schemes, 2 tests / 18.05s, with
successful selected relay pairs and positive sent/received bytes. Both bounded
test services were stopped and verified inactive. These results do not turn
PixelScan's still-negative fonts/canvas classification into a pass.

### Vectorized candidate `5f7fb6e` acceptance

The extracted, sandboxed candidate completes the full Linux headful suite:
**131 passed, 19 skipped in 313.32s**. Both host Fontconfig variants were
configured. The skips include macOS-only experiments, opt-in timing/diagnostics
and separately configured real-network/API tests; they are not passes.
Against Mac reference binary `945d653`, all **120 common PNG/color/seeded-corpus
observations and 32 raster primitives match**, with no differing hashes.
The 40 stage and 84 managed vertical-metric golden assertions also pass.
This does not replace a rebuild/acceptance of the latest changes on Mac or Windows.

Separate real-network runs on this same artifact pass:

- Controlled IPv4/IPv6 STUN: 4 independent positive-control packets, 0 browser
  packets across HTTP/SOCKS5, 0 capture drops.
- TURN/TLS: both HTTP/SOCKS5 echo tests pass, with two selected relay pairs per
  test and positive sent/received bytes. The test page does not request relay-only
  policy; the product enforces it. Public certificate verification stays enabled.
- Direct-TCP exclusion at the controlled TURN endpoint: 7 positive-control
  packets, 0 browser direct packets, 254 proxy-origin inbound packets, 0 drops.
  The first wrapper attempt rejected an incorrect assumed port before starting
  the service. The successful capture uses the existing endpoint's port 443 and
  filters both its address and port, excluding unrelated HTTPS traffic.
- Both bounded STUN/TURN services were verified inactive afterwards.

The managed QA profile runs this exact extracted executable and passes all
35 built-in verification checks. Its optional Remote Control child reports
`401 invalid_auth`; the browser/profile itself is running and independently
verified. No credential workaround was used. Navigation HTTP identity matches
window/Worker identity, without exposing the raw runtime patch version.

An unmodified PixelScan scan still reports **inconsistent / Masking detected**,
with no proxy or automated-behavior classification. A separate instrumented
diagnostic again finds only the fonts and canvas predicates false; the other
11 predicates are true. The diagnostic is not the unmodified screenshot proof.
Canvas protection remains enabled; no checker-specific exceptions were added.
PixelScan, the forced-baseline performance risk and latest Mac/Windows binary
acceptance remain open. This is not a merge or release recommendation.

The separate PixelScan WebRTC checker shows a red warning despite submitting
empty ICE-address lists. Its success condition requires the HTTP public IP
to occur in a STUN/TURN list; the displayed IP is the proxy address, not the
QA host. Source and decoded request/response evidence are documented in the
[WebRTC checker diagnosis](pixelscan-webrtc-empty-candidates-2026-09-15.md).
Do not interpret that UI warning as a demonstrated host-IP leak in this run,
or claim that the site returned a green result.

## New translucent blend matrix: unresolved half-float/CPU difference

`test_canvas_blend_matrix.py` covers 26 Porter-Duff and artistic blend operations
over a translucent three-stop gradient and a fractional ellipse. It records
both unorm8 and float16 backing stores, DOM/Offscreen contexts, both readback
hints and default/baseline CPU dispatch: **416 observations per host**.
Actual context type, repeated-read equality, nonuniform alpha and distinct
operation outputs guard against blank or unsupported fixtures. No fonts or
external sites are involved. Pixel protection remains enabled.

The existing Mac `945d653` reference passes the within-host matrix in 21.54s.
Linux `c067039` fails its CPU-path equality assertion in 39.20s. An earlier
Linux invocation had an inaccessible `/root` working directory and is not
evidence; the reported run uses the readable QA checkout as non-root `builder`.
Neither invocation disabled the browser sandbox. This is the same extracted
Linux executable, not a new engine candidate.

Diagnostic inspection of the failing report (not accepted by the strict
cross-report comparator) finds 120 cross-host differing observations:

| Path | Differing observations |
| --- | ---: |
| Default CPU, unorm8 | 4 (color-burn) |
| Default CPU, float16 | 12 (color-burn, hue, saturation) |
| Baseline CPU, float16 | 104 (all 26 operations) |
| Baseline CPU, unorm8 | 0 |

DOM/Offscreen and readback hints agree within each CPU path. The source contains
two concrete leads in `SkRasterPipeline_opts.h`: baseline `to_half` truncates
mantissa bits and flushes subnormals while AVX2/ARM64 use hardware conversions;
blend operations including color-burn and hue/saturation use architecture-
dependent approximate reciprocal helpers. These are investigation leads, not
a validated fix. The new failing regression is retained without xfail or skip.

Reports: `linux-blend-matrix-049.xml` and `mac-reference-blend-matrix.xml` in
the local `clawbrowser-linux-c067039` evidence set. Next steps are isolated
numeric conversion regressions, an exact portable conversion candidate, then
rebuilding and rerunning this matrix before broader acceptance/performance.
The latest Mac/Windows engine builds and PixelScan remain separate open gates.

### Patch 050 candidate: exact baseline binary16 conversion

The candidate replaces only raster paths lacking hardware half conversion with
round-to-nearest/ties-to-even float-to-half and exact half-to-float conversion.
It preserves signed zero and subnormals, handles overflow/infinities and quiets
NaNs. AVX2/ARM64 hardware branches are unchanged. It does not change Canvas
policy, formats, noise, domain handling or the approximate reciprocal helpers.

`skia_half_test.sh` extracts the actual added header from patch 050. Its
independent reference decodes half values with `ldexp` and chooses the nearest
representable half by searching that decoded table. It checks all 65,536 half
encodings, neighbors of every positive finite half midpoint with both signs,
overflow boundaries and one million seeded float bit patterns. On macOS
Apple Clang and Linux GCC it passes **65,536 decodes / 1,256,004 encodes**.
The old truncating/flushing expression disagrees in 400,946 applicable cases,
providing a negative control. The test is added to the numerical CI matrix.

Browser compilation/acceptance and baseline float16 performance remain pending
for this candidate. The opt-in bitmap timing fixture now accepts a float16
backing-store selection and asserts that the requested format was obtained.
Do not infer that the 120 observed blend differences are resolved from the
standalone numerical result.

The rebuilt Linux candidate is now
`b054b3e2836da1419b029168a41ec561155cc2fc4539bbcdd594dba76552b71d`
(58 build steps, 309.31s), extracted from archive
`0072c9f311b35d1d8787b00b7c8e4d1c56de3368d49db58d28bed1c1ae38b622`.
The unchanged headful blend regression still fails in 39.71s, but diagnostic
comparison shows **16 differences instead of 120**: all 208 baseline CPU
observations now match the Mac reference. Remaining differences are the default
CPU path's unorm8 color-burn and float16 color-burn/hue/saturation (four contexts
each). Thus the baseline half-conversion discrepancy is resolved in this
fixture; the overall blend assertion and release acceptance are not resolved.
The report is retained as `linux-blend-matrix-050.xml` alongside the earlier
failing report. Float16 old/new ABBA timing is a separate pending run.

The float16 ABBA timing run subsequently completed all 16 opt-in cases without
concurrent build/test CPU work. Per-iteration medians (milliseconds):

| CPU / policy | Old 049, two runs | New 050, two runs |
| --- | --- | --- |
| Default / native | 0.1967, 0.1838 | 0.1804, 0.1814 |
| Default / protected | 0.5783, 0.6019 | 0.5627, 0.5850 |
| Baseline / native | 0.6005, 0.6071 | 1.0382, 1.1033 |
| Baseline / protected | 1.0047, 0.9798 | 1.4511, 1.4552 |

There is a substantial baseline regression (about 47% for protected mode),
not a performance-neutral fix. Default-path ranges do not show a comparable
regression. Optimize the baseline common normal/zero lanes with exact vector
bit arithmetic while retaining the tested scalar special/subnormal handling,
then repeat both numerical and browser/timing gates. Do not drop subnormals or
weaken correctness to recover speed. This candidate remains unsuitable for
release acceptance.

Patch 050's follow-up candidate adds an SSE2-only common path for normal and
zero lanes, using integer bit operations for exact rounding/conversion. A group
containing a subnormal, infinity, NaN or out-of-range float still uses the
scalar reference. Helpers are forced inline to avoid cross-target out-of-line
selection. The numerical test now verifies the actual extracted SIMD helper
with **262,144 mixed decode lanes and 5,024,016 mixed encode lanes**, alongside
the previous scalar/reference tests. Linux GCC passes with `-msse2 -mno-avx`;
macOS ARM64 retains scalar coverage. Rebuilt-browser performance is still
required before claiming the slowdown fixed.

The SSE2 follow-up was built and staged as `exact-half-050-v2`: ELF SHA-256
`2cbd2d711f672315f2c5ad7bb0e8ca2a474bb7864298010425652497de899b14`,
archive SHA-256
`ed2005aa0985db289beb1fe108142a333ce250875485c93f5ba53710bb39d78b`.
The headful sandboxed blend run completed in 38.15s. All **416 observations
are identical to 050 v1**, confirming that the SIMD optimization preserved
this fixture's pixels. The strict regression still fails: the same 16
default-CPU observations differ from the Mac reference (color-burn in both
formats, hue/saturation in float16). This is not overall acceptance. Comparative
ABBA performance is evaluated separately.

The 050 v2 ABBA run completed all 16 timing cases with no concurrent build.
Per-iteration medians (milliseconds):

| CPU / policy | Old 049, two runs | New 050 v2, two runs |
| --- | --- | --- |
| Default / native | 0.1887, 0.1889 | 0.1737, 0.1830 |
| Default / protected | 0.5459, 0.5548 | 0.7153, 0.7097 |
| Baseline / native | 0.6112, 0.6373 | 0.6998, 0.6572 |
| Baseline / protected | 0.9638, 0.9423 | 1.2154, 1.1698 |

Baseline conversion improved relative to v1, but this is **not a resolved
performance gate**: protected mode is still about 25% slower on baseline CPU
and 29% on default CPU in this run. The earlier v1 default-path result must
not be substituted for this artifact's measured result. Reports are retained
as `half-abba-050-v2-cpu{0,1}-{0,1,2,3}.xml`.

Patch 051 is a separate experimental candidate replacing `rcp_fast`'s
architecture-dependent estimate/refinement with vector division on all paths.
It leaves `rsqrt` and existing direct `rcp_precise` callers unchanged. The
remaining failing blend cases use this reciprocal helper. Compilation and
the unchanged strict blend matrix are required to test this hypothesis;
cross-platform correctness and performance are not yet established.

The 051 Linux candidate subsequently built successfully (57 steps, 303.18s):
ELF `2e9d440718b4ea669c770b582b1a535b608e32a46c49951ecc15b106f275a857`,
archive `dd5ae67e31e9732a4f8ee4275c2e910962fd360aa8839f67fd8ce8bb65386676`.
The unchanged sandboxed headful blend regression now **passes in 38.20s**.
All 416 observations agree across CPU/context/readback-hint variants and match
the existing Mac reference: **zero cross-host differences**, down from 120
on 049 and 16 on 050. This validates the reciprocal hypothesis for this
fixture. Report: `linux-blend-matrix-051.xml`.

This is not universal rendering/release acceptance. The broad Linux suite is
the next gate, the protected float16 performance cost is unresolved, and the
Mac reference has not been rebuilt with 050/051 because local build access
remains blocked by the Xcode license prerequisite.

The extracted 051 artifact then passed the broad headful Linux suite:
**135 passed / 21 skipped in 410.47s** (`full-suite-051.xml`). The same two
standalone diagnostic modules were excluded; no acceptance test was weakened.
Skips include opt-in timing/step diagnostics, macOS-only checks, external
backend/proxy/STUN/TURN checks needing their separate configuration, and a
non-native-policy fixture. They are not counted as passes or fresh network
proof. Strict comparisons against the matching existing Mac reports passed
**614 comparable observations**: 120 corpus/format, 32 primitives, 16 SVG,
30 bitmap and 416 blend. Other unmatched observations are not cross-host proof.

051 float16 ABBA timings also completed all 16 opt-in cases:

| CPU / policy | Old 049, two runs (ms) | New 051, two runs (ms) |
| --- | --- | --- |
| Default / native | 0.2107, 0.1775 | 0.1725, 0.1894 |
| Default / protected | 0.5870, 0.5612 | 0.7720, 0.7157 |
| Baseline / native | 0.6037, 0.6100 | 0.6449, 0.6531 |
| Baseline / protected | 0.9726, 0.9626 | 1.2155, 1.2007 |

Thus the protected float16 cost remains approximately 25–30% in this fixture;
correct blend output does not imply performance neutrality. Ordinary unorm8
timing is a separate experiment, not interchangeable with these numbers.

Managed-profile retest of the same 051 executable passed internal Verify
**35/35**. `/proc` executable resolution confirmed the extracted candidate,
not another browser. The ordinary, uninstrumented PixelScan run still shows
**Inconsistent / Masking detected**, with **No proxy detected / No automated
behavior detected**. Screenshot and private local summary were preserved as
`pixelscan-exact-reciprocal-051.{png,json}`; the image was visually inspected.
No website-specific renderer exception or disabled Canvas protection was used.
Cross-host fixture equality must not be reported as a green PixelScan result.
The previous instrumented font/Canvas predicate diagnosis was not rerun here.

Ordinary unorm8 ABBA also completed 16 cases. Native/default old/new ranges
were 0.1542–0.1600 / 0.1526–0.1835 ms; protected/default was
0.5273–0.5453 / 0.6858–0.6998 ms. Native/baseline was
0.3350–0.3461 / 0.3350–0.3412 ms; protected/baseline was
0.7198–0.7262 / 0.8693–0.8811 ms. Therefore the protected-mode slowdown is
not confined to half-float backing stores.

An isolated `perf` CPU-clock comparison (2,629 old samples, no lost samples)
identified `ApplyDeterministicCanvasNoise` as the largest hotspot: 50.10% of
old CPU samples versus 58.19% of new samples. Importantly, disassembling that
function found **302 identical normalized instructions** in both artifacts,
with the same 0x483-byte size. Only placement/relocations differ. This does
not establish that half conversion or division caused the protected-mode
slowdown; code layout/cache effects remain a hypothesis. Next optimize the
generic per-pixel/per-channel dispatch while preserving every output byte,
and compare against both artifacts rather than changing noise semantics.

Fresh controlled networking on 051 passed as well: independent IPv4/IPv6 STUN
controls produced four captured records, and browser HTTP/SOCKS5 phases
produced zero direct STUN records. TURN/TLS echo passed both proxy schemes
with positive relay bytes and verified TLS. The direct-TCP positive control
captured seven packets; the browser phase captured zero direct and 300
proxy-origin packets, with zero kernel drops. These are controlled-endpoint
claims, not proof about all Internet routes. Both bounded network services
were confirmed inactive after the tests.

### Format-specialized noise traversal candidate

The noise hot loop now selects a compile-time storage format once per buffer
rather than executing the format switch per RGB component. Coordinate seeds,
PRNG sequence, component canonicalization, padding/alpha preservation and
layout validation are unchanged. A reference test retains the old generic
traversal and compares all bytes in 1,024 cases across the four formats,
including arbitrary floating-point patterns, misalignment, row padding,
signed origins, zero/max/random seeds and repeated application. All ten
CanvasNoise C++ tests pass.

Candidate `noise-dispatch-052` built in 300.80s (49 steps): ELF SHA-256
`83a4144b3bdbadfc1393ae82bb332b0e1c935d5afd0c12db19536d92ad831fb4`,
archive SHA-256
`a652b1561397a36a5a8edd1c939d7407e0448efc82700bcd581d316288e2f394`.
The strict sandboxed headful blend matrix passes in 36.65s. Comparative
performance and the broad regression suite remain separate pending gates;
unit correctness alone does not establish a speedup.

The unchanged strict comparison confirms all 416 blend observations match the
Mac reference. All 32 ABBA timing cases then passed, with no concurrent build
or other raster workload. Protected-mode medians per iteration:

| Format / CPU | Old 049, two runs (ms) | New 052, two runs (ms) |
| --- | --- | --- |
| unorm8 / default | 0.5333, 0.5075 | 0.4204, 0.4010 |
| unorm8 / baseline | 0.7257, 0.7177 | 0.5824, 0.5811 |
| float16 / default | 0.5477, 0.5545 | 0.4247, 0.4320 |
| float16 / baseline | 0.9494, 0.9624 | 0.8728, 0.8795 |

The protected regression is reversed in this workload: approximately 20–22%
faster than 049 on default CPU, 8–19% faster on baseline CPU. This is not a
universal browser speedup. Native/unprotected float16 baseline still costs
0.6358–0.6412 ms versus 0.5750–0.5834 ms on 049, consistent with remaining
conversion overhead, while native/default timing ranges overlap. Reports are
`noise-abba-052-{unorm8,float16}-cpu{0,1}-{0,1,2,3}.xml`.
The full 052 suite is a separate pending acceptance gate.

The extracted 052 artifact subsequently passed the full headful Linux suite:
**135 passed / 21 skipped in 388.82s**. The same two diagnostic-only exclusions
were used. All **714 comparable protected observations are identical to 051**,
and all **614 comparable observations match the existing Mac references**.
This closes the broad Linux regression for this output-preserving optimization;
it does not rebuild/certify macOS or Windows, close Desktop OAuth, or change
the previously observed red PixelScan classification.

### Dynamic web-font fallback (2026-09-16)

Candidate 052 passed a new headful font-cache invalidation regression using the
actual packaged, manifest-hash-checked Tinos asset as an in-memory web-font with
ASCII-only `unicode-range`. The positive Latin control switches to a custom font
with different width; Arabic, Devanagari, Thai and the selected emoji sequence
retain exactly their prior bundled-font provenance and widths. Removing the
FontFace from `document.fonts` restores all five baseline observations.

The test passed with default, isolated Noto, and isolated Liberation host
catalogs (1.58s / 1.57s / 1.57s). All 15 text/state observations agree across
these Linux configurations. Reports: `dynamic-font-fallback-052.xml`,
`dynamic-font-fallback-052-noto.xml`, and
`dynamic-font-fallback-052-liberation.xml`. This covers a previously untested
load/remove transition, not every script or web-font configuration. No runtime
change was needed. The equivalent macOS run remains pending; this result does
not resolve PixelScan's classification or certify all platforms.
