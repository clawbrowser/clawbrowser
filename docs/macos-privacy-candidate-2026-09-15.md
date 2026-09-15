# macOS candidate: build success, acceptance blocked

The local arm64 build of Chromium 151.0.7922.109 at
`28a7a6c409e03c701d3474ef9e3b1f0be6249039`, with the candidate overlay and
patches through 037, completed all 57,990 Ninja actions. The staged QA app is
ad-hoc signed, not an installed or released product. Its launcher SHA-256 is
`7d33caded59b4f8e741db94a7127f8436188ee19a498f7c187e698d90ba9c719`;
this identifies the launcher, not the complete framework payload.

## Results

- C++: 214 passed, zero failures.
- Selected headful integration: 58 passed, 5 failed, 2 skipped in 195.38s.
- Skips: native canvas policy fixture and unconfigured external STUN endpoint.
- Portable blocked-family fallback provenance passed. This is not proof that
  macOS uses the Linux bundled catalog; it does not.
- Four failures: native/override drawing corpus and color-format matrix.
  DOM, OffscreenCanvas and Worker agree within each `willReadFrequently`
  setting, but false and true produce different hashes. Disabling Skia runtime
  optimizations did not remove the difference.
- Diagnostic-only `--disable-accelerated-2d-canvas`, with all original assertions
  and protection retained: all four matrices passed in 40.04s. This isolates the
  accelerated/software rendering path distinction; it is not a product fix or
  permission to disable GPU globally.
- The fifth failure was the expected-font test's width heuristic. Courier New
  was not distinguished from generic monospace. Replacing that heuristic with
  actual CDP glyph-family checks passed for every expected family in 1.90s.
  Linux's existing bundled-catalog provenance assertions are unchanged.

Evidence is retained in the local QA task's `evidence/20260915-054018`:
`cpp.xml`, `candidate-gates.xml`, `software-canvas-diagnostic.xml`, and
`font-provenance.xml`. The original failing results are preserved.

## Remaining gates

Choose and validate a general, policy-scoped normalization strategy for canvas
rendering, including its performance impact. Do not add detector-specific
exceptions or weaken assertions to mask the path difference. Repeat the full
suite against any changed binary, and complete managed identity, real network
and platform gates. PixelScan's Linux fonts/canvas conditions remain unresolved.
These results supersede the older documents' “macOS binary pending” status,
but do not make the PRs ready for merge or establish Windows/Linux ARM support.

## Patch 038 follow-up

Policy-scoped software Canvas2D rasterization was built and tested on the same
Mac. It applies only when canvas spoofing is enabled and policy is `override`.
Native mode retains Chromium's backend choice; WebGL and readback noise are not
changed. No detector, canvas size or color exceptions are introduced.

The new framework SHA-256 is
`583c12a01c028b6b366eea28a59cef7811b1e26024a9a75022e600c79e805fbd`.
Selected gates: **61 passed, 2 skipped, zero failures in 147.14s**, including
the unchanged protected corpus and color-format equality assertions, font
provenance, PNG/origin-clean/source-snapshot, surfaces and proxy tests.
Skips are the same fixture/external-STUN conditions above. No software-forcing
command-line switch was used. Native equality is deliberately not claimed.

A bounded, single-host readback-heavy timing diagnostic (five samples after
warmup; eight drawing/readback iterations per sample) gave these medians:

| Override drawing | Previous, frequent=false | Patch 038, frequent=false | Patch 038, frequent=true |
|---|---:|---:|---:|
| 256×256 | 13.5 ms | 3.7 ms | 3.8 ms |
| 1920×1080 | 333.3 ms | 107.7 ms | 107.6 ms |

Native medians were similar before/after (1920×1080 default: 262.1/269.0 ms).
This workload favors software readback and does **not** establish animation,
GPU-heavy, transferred-canvas presentation or overall application performance.
Those and the full/platform/network gates remain necessary. Evidence:
`patch038-gates.xml` and `raster-cost.json` alongside the original results.

## Explicit remaining macOS font boundary

The full-suite CDP provenance artifact records actual system fallbacks even
while the blocked-family equivalence test passes: Arabic uses Geeza Pro, CJK
uses PingFangSC-Regular, Devanagari uses KohinoorDevanagari-Regular, and emoji
uses AppleColorEmoji. Direct requests for Papyrus/Copperplate match the generic
baseline, but generic character fallback still depends on this host's CoreText
catalog. CDP observation is privileged diagnostic evidence, not proof that a
web page can enumerate these exact names. Their glyph metrics/rendering remain
an unclosed host-dependent surface. Do not claim the Linux bundled-catalog
isolation has been implemented on macOS or that patch 038 normalizes font
rasterization across operating systems. A controlled catalog/fallback strategy
must preserve language and emoji coverage; simply rejecting these glyphs is
not an acceptable fix.
