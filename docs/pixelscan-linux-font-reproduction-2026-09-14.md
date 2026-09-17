# PixelScan Linux font-classification reproduction

## Observation

On September 14, 2026, stock Google Chrome 153.0.8010.36 on Ubuntu 26.04 under
Xvfb returned **Masking detected** and **No automated behavior detected**.
`navigator.webdriver` was false. There were no ClawBrowser patches, fingerprint
overrides, JavaScript property replacements, or proxy in this reference run.
Chrome used a fresh temporary profile and its normal sandbox. CDP was used to
navigate and collect results; this was not an uninstrumented manual session.

PixelScan `/s/api/co` returned HTTP 200:

```json
{
  "status": "ok",
  "value": {
    "comparedResult": {"match": true, "sameGroup": false, "os": "linux"},
    "osFontsStatus": false
  }
}
```

The corresponding input had `platform="Linux x86_64"` and `canvas=true`.
The returned font result is a required predicate in the loaded frontend's
masking calculation. This reproduces the negative font result without the
ClawBrowser implementation. It does not establish that all detector warnings
are false positives, or that every Linux desktop behaves this way.

## Reference environment

Chrome was launched directly with a nonzero local debugging port, a new
user-data directory, `--no-first-run`, and `--no-default-browser-check`.
No `--enable-automation`, headless flag, no-sandbox flag, or stealth script was
used. The ordinary Playwright-launch control also returned the same font result,
but had webdriver=true and is not the primary reference above.

The reference font configuration included the server's installed fonts plus
Ubuntu archive packages extracted into a separate directory (not installed
system-wide):

- fonts-ubuntu 0.869+git20240321-0ubuntu2
- fonts-dejavu-core 2.37-8build1
- fonts-freefont-ttf 20211204+svn4273-4build1
- fonts-noto-cjk 1:20240730+repack1-1build1
- fonts-noto-color-emoji 2.051-1build1

The original, smaller system font set also produced osFontsStatus=false.
Adding fonts did not make this reference pass. No personal account was used.

## Questions for the detector maintainer

A second control used official Chrome for Testing **151.0.7922.77**, matching
the product's Chromium 151 build branch rather than the newer installed Chrome
153. It reproduced `osFontsStatus=false`, `canvas=true`, OS match true and
webdriver=false with the same richer font set. Both navigation and teardown
completed successfully. This rules out the 151-versus-153 difference as the
sole explanation for the observed font result; it is not an uninstrumented
desktop or all-Linux claim.

The download URL was resolved from Google's
[known-good versions manifest](https://googlechromelabs.github.io/chrome-for-testing/known-good-versions-with-downloads.json).
The downloaded Linux ZIP SHA256 is
`60a324a6e1d27b20f2035a2cdaf71641a739fe1f5571f63794773225820bce8a`.
Evidence directory: `stock-cft151-full-fonts`. The screenshot also contains
unrelated direct-server network/timezone warnings and unavailable WebGL fields;
it must not be presented as a fully consistent stock desktop. The isolated
font observation comes from `/s/api/co`, separately from those UI warnings.

1. Which Linux/font-rendering invariant causes this response to be false?
2. Are current Ubuntu font versions and Chrome 153 covered by the classifier?
3. Can the UI distinguish an unfamiliar font signature from an actual
   inconsistency or detected modification?

The canvas font probe labels each distinct raster result with its first candidate
font name. Such labels are not necessarily successful local font lookups: in the
ClawBrowser control, Abyssinica SIL was reported while its local FontFace lookup
failed. Please clarify how fallback-equivalent candidate names are interpreted.

## Evidence and scope

### Exact protected-candidate predicate diagnosis

A fresh normal run on the extracted a984314 archive with the real QA backend
loaded both AmIUnique and PixelScan (HTTP 200). PixelScan again returned the
negative font result with `canvas=false`. The site's ordinary telemetry did
not contain the complete masking predicate list.

A separate **instrumented diagnostic**, `archive-mask-locals-a984-r2`, read the
already-computed booleans at the frontend's masking dispatch using a conditional
debugger breakpoint. It did not replace classifier inputs or change its return
value. This instrumentation is not an uninstrumented acceptance run. The first
attempt timed out and is not evidence of a completed scan; the second completed.

| Predicate | Observed |
| --- | --- |
| Font classifier | false |
| Canvas exact-color check | false |
| WebGL status and fixed red WebGL rectangle | true, true |
| Worker comparison and iframe comparison | true, true |
| HTTP/JS User-Agent and Client Hints | true, true |
| Navigator object and OS match | true, true |
| Platform, locale and hardware checks | true, true, true |

Thus only fonts and canvas failed among the predicates evaluated in this run.
This is narrower than claiming all possible fingerprint checks pass, or that
the font classifier is necessarily wrong in every environment. No additional
runtime patch, font expansion, or disabling of canvas protection was made to
obtain this observation. The remaining engineering choice is a validated
normalized-rendering strategy versus the present detectable pixel protection;
turning the latter off alone would not resolve the font result.

### Cross-context canvas normalization experiment

The diagnostic matrix in `test_isolated_canvas_control.py` now also compares
complete RGBA buffers from Window Canvas, main-thread OffscreenCanvas and a
Worker OffscreenCanvas. Each runs with default and `willReadFrequently` contexts,
two host-font configurations and normal/disabled Skia runtime optimizations.
The drawing includes multilingual text, a gradient, subpixel Bezier curves,
rotation, alpha compositing, ellipse strokes and a blurred shadow.

Both explicit native and protected policies produced internally identical
hashes across their respective 24 combinations on the a984314 Linux archive.
This does not mean native and protected hashes are identical to each other.
The original six-launch font/raster control also passed. The complete file
reported 3 passed in 31.18 seconds (`canvas-cross-context-a984-final.xml`).
Array size and nonuniformity assertions prevent empty drawings from satisfying
the comparison. The tested Chromium source consumes `kDisableSkiaRuntimeOpts`
in `content/common/skia_utils.cc` and forwards it to renderer/GPU processes;
this is not an invented or ignored test flag. This remains one physical x86_64 machine,
not cross-processor, cross-architecture or cross-platform acceptance.

No heuristic exempts PixelScan's colors, canvas dimensions, domain, or probe.
No product policy changed. A future normalized-renderer policy needs broader
hardware coverage and a defined profile-identity contract before replacing
per-profile noise; this experiment is supporting evidence, not that approval.

The follow-up corpus retains the original drawing and adds four seeded recipes,
each with 16 subpixel Bezier shapes, radial gradients, randomized alpha/shadows
and four compositing operators. Seeds are fixed and all five drawings must
produce distinct hashes. Each recipe is compared across the same 24 combinations
per policy: 240 complete-buffer observations overall. The corpus passed all
three tests in 100.59 seconds on the same extracted archive
(`canvas-seeded-corpus-a984.xml`). This broadens drawing coverage but does not
remove the single-physical-host limitation.

Subsequent format coverage passed sRGB/Display-P3, unorm8/float16 backing
stores, both pixel read formats, and PNG alpha/export checks across Window
and Worker. The combined format/PNG/origin-security run passed six tests in
25.07 seconds (`canvas-formats-origin-final-a984.xml`); details and boundaries
are in the Linux privacy acceptance report. These tests do not alter the
PixelScan observation or establish normalized rendering on other hardware.

Lab evidence directory: `stock-minimal-launch-full-fonts` contains the screenshot
and report captured before browser teardown. The process subsequently reported
a temporary-profile cleanup race; the recorded page and API results were already
saved. Do not count this as a clean teardown test. Network/IP observations in
this direct-network stock reference are not protected-product leak tests.

ClawBrowser's separate Linux Canvas2D raster-path fix passed 215 C++ tests and
54 headful tests (one existing skip), but does not make PixelScan green.
Its production canvas protection remains enabled. This report has not been
sent to an external party and is not a release or merge approval.

## Fresh managed 065 result (September 16)

The existing `us-browserscan-reduced-qa` profile was restarted through managed
`nbc` against the 065 executable, without rotating its identity. The running
`/proc/PID/exe` matched the expected extracted binary and SHA256
`767337994307176a03117a32b234277d59bd45ef66245445cb0180919c2305a4`.
Internal verification passed 35/35 checks. A separate live pixel canary confirmed
the explicitly selected experimental normalized-Canvas policy; this is **not**
the default protected policy. The QA traffic endpoint returned unavailable,
not exhausted; live proxy verification succeeded. No auth/config files were read.

The ordinary PixelScan scan completed and still showed **Masking detected**,
**No proxy detected**, and **No automated behavior detected**. The screenshot
was saved before any classifier instrumentation. A subsequent diagnostic
observed **fonts=false and the other twelve predicates=true**, including
Canvas, UA/Client Hints, Worker and WebGL. Classifier API responses were HTTP 200.
This is the 065 observation, not an extrapolation from the previous 064 run.

On the same 065 profile BrowserScan displayed **100% fingerprint authenticity**,
with populated browser/IP/hardware fields and matching reduced HTTP/JS UA.
Its DNS widget was not independently accepted from the page; controlled routing
evidence remains separate. Neither the score nor the site's proxy label proves
network safety. Both normal screenshots were visually reviewed.

Evidence: `pixelscan-065-reduced-ua.{json,png}`,
`pixelscan-mask-065-reduced-ua.json` (instrumented, separate),
`checker-browserscan-065-reduced-ua.{json,png}`, and `managed-runtime-065.json`.

### Family names are not necessarily local face names

A diagnostic initially found that `local("Noto Sans Kannada")` and
`local("Noto Sans Myanmar")` failed although requesting those CSS families
changed the raster. This **did not establish a runtime inconsistency**:
metadata from the actual bundled files identifies full names ending in
`Regular` and PostScript names `NotoSansKannada-Regular` and
`NotoSansMyanmar-Regular`. Both forms loaded successfully in the same managed
profile. The short family-only probe had tested the wrong face identifier.

The controlled headful 065 regression now compares each CSS family against
both loaded full-name/PostScript faces across three generic fallbacks, Latin
and Kannada/Burmese text, and both Canvas modes: **48 complete pixel/metric
comparisons pass**. The absent-name tests additionally try full/PostScript-style
variants for Abyssinica SIL, Papyrus and the missing-name control: **54 fallback
comparisons pass**. The combined file passed six tests in 7.97s
(`font-fullname-065.xml`). This strengthens the diagnostic; no runtime patch
was needed and PixelScan's font condition remains unresolved.

Live diagnostic evidence `font-labels-065-fullnames.json` confirms seven of
the ten tested families are available through a valid face name; the three
absent names are fallback-equivalent in all nine comparisons each. The earlier
family-only observation is retained as `font-labels-065.json`, not presented
as evidence that the two bundled Noto fonts are missing.

## Managed 067 website confirmation (September 17, UTC)

After the previous QA display expired and the managed profile reported stopped,
the same profile was started through `nbc` with the extracted 067 runtime.
No identity rotation or auth/config inspection was performed. Internal verify
passed 35/35. The live process executable and SHA256 matched the 067 artifact
in [the DoH boundary report](managed-doh-proxy-boundary.md); a live canary
confirmed experimental normalized Canvas (zero changed pixels).

An ordinary PixelScan run completed before classifier instrumentation:
**Inconsistent / Masking detected**, **No proxy detected**, and **No automated
behavior detected**. A separate instrumented run observed **fonts=false** and
the other twelve predicates true, with classifier API HTTP 200 responses.
The displayed font hash `dd840e733d45ca22e94dec4a106d5383` and five font labels
match the 065 observation. The DoH correction therefore does not resolve the
remaining font-classifier result; no site-specific exception was introduced.

BrowserScan on the same running 067 profile displayed **100% authenticity**
after hardware and font fields populated, with matching reduced HTTP/JS UA,
Canvas `F49629D5`, WebGL `6037CFEC`, and font hash `40C579C5`, matching 065.
Its WebRTC/STUN widgets said disabled; working proxied relay is established by
the separate controlled TURN tests, not that label. Its DNS widget displayed
resolver addresses, but these are not independently attributed to host versus
proxy from this screenshot and do not establish DNS leak acceptance.

The initial BrowserScan capture occurred while hardware/font fields were still
empty despite a displayed 100% score. It is **not completed evidence**. The
capture readiness check was strengthened to require Canvas hash, renderer,
screen dimensions and nonempty font results; the completed capture is below.
No reload or identity change was needed to let asynchronous results populate.

Evidence (normal screenshots visually reviewed):
`managed-runtime-067.json`, `pixelscan-067-doh-boundary.{json,png}`,
`pixelscan-mask-067-doh-boundary.json` (instrumented only), and
`checker-browserscan-067-complete.{json,png}`.
The incomplete initial capture is retained separately as
`checker-browserscan-067-doh-boundary.{json,png}`. This is Linux QA evidence,
not default-policy PixelScan acceptance or all-platform release approval.

### Independent inline-SVG font-oracle regression

The existing SVG-image-to-Canvas test did not cover inline SVG's direct text
metric APIs. A new investigation checked whether these could recover a blocked
host font despite ordinary HTML and Canvas isolation. This is a general privacy
boundary test, not a claim that PixelScan uses these particular APIs.

`test_svg_font_metrics.py` compares computed text length, bounding box,
per-character start/end positions, extents, substring lengths and rotations,
plus HTML width, for blocked-family requests versus their explicit generic
fallback. It covers three family requests, three generics, Latin/Arabic/mixed
Indic-CJK strings, normal/bold-italic styles, two Linux host-font configurations,
and protected/experimental-normalized Canvas: **216 comparisons**. All agree
with fallback and remain identical between host-font configurations.

The independent unproxied/no-fingerprint control proves that those configurations
actually change SVG layout: generic widths are `[173, 180, 238]` with Noto and
`[164, 166, 238]` with Liberation. Nonzero bounds, character counts and distinct
generic widths reject empty or constant-metric false successes.

All three headful, non-root, sandboxed tests passed on 067 in **7.45s**
(`svg-font-metrics-067-controls.xml`). No new runtime defect was reproduced and
no product patch was needed. This adds regression coverage; it does **not**
resolve or reclassify PixelScan's remaining font warning.
