# Cross-platform protected raster comparison

## Current acceptance boundary

This report preserves the investigation chronologically; initial failures below
are not the latest result. Linux `65904c2` completes the headful run with
131 passed and 19 skipped, matches all 120 common Mac corpus/format hashes and 32
primitives, and passes the controlled STUN/TURN packet tests. Exact artifact
hashes, timings and caveats appear in the later sections.

**Not ready for merge/release:** forced-baseline CPU cost remains substantial,
PixelScan's font/Canvas classification remains negative, and the latest changes
still need Mac/Windows binary acceptance. Do not confuse reference Mac results
with a rebuild of these latest patches, or site diagnostics with a green scan.

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
