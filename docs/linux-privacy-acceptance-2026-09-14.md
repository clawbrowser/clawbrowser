# Linux privacy acceptance — 2026-09-14

Status: **Draft; not release approval.** No production deployment or merge.

## Worker platform correction (10:34 UTC)

A new Dedicated Worker regression failed on the previously tested binary:
the fixture's page reported `MacIntel`, but its worker reported the host's
`Linux x86_64`. The other seven compared identity fields agreed. This is a
real cross-context platform disclosure, not an explanation of PixelScan's
masking verdict for the same-platform Linux profile.

Commit `83013e04ac7a1ff8189f63247e651f6cf6772091` adds the fingerprint platform
override in `NavigatorBase::platform()` and the regression test. Incremental
Chromium build completed successfully in 5m36s. New executable SHA-256:
`4880b44c98a172232b2ef81bd1bd0c8448b95409f5d81c50d3821eba4f95c5d7`.
The prior relocated artifact was retained; the new copy contains refreshed
binary, snapshots and resource packs. It uses the same pinned font bundle.

Retest results on Linux, sandbox enabled:

- Dedicated Worker regression and adjacent navigator checks: **8 passed**,
  headful, 9.20s.
- Entire `test_surfaces.py`: **43 passed, 1 skipped**, headful, 56.84s. The
  skipped native-canvas case is inapplicable to this override-policy fixture.
- Compiled C++ suite: **211 passed**.
- Real candidate backend/API/proxy integration: **2 passed**, 5.214s, using
  backend `e876e082f36ec743c23a910383ab733a9bfd0f8f`.

The first relocated launch encountered the host's missing per-path AppArmor
user-namespace permission. Adding the same exact-path rule as the previous
artifact resolved this environment failure without disabling Chromium sandbox.
This is not a signed release package, desktop UI
acceptance, or cross-platform verification. Remaining release gates below still
apply. Earlier external-site screenshots were captured on the older binary.

### Additional worker contexts (10:53 UTC)

The identity regression now covers Dedicated, Shared and Service Workers.
All three passed headful on `4880b44` in 3.57s. Separate negative-control runs
on `93b36fd` reproduce `MacIntel` versus host `Linux x86_64` in both Shared and
Service Workers (the original Dedicated negative was already recorded).
The service worker runs from the integration fixture's loopback HTTP origin;
its registration is removed after the check. This is a real renderer/runtime
test against a controlled fixture, not a desktop account or external-site E2E.
This extension changes tests only; the verified binary is unchanged.

### QA archive round trip

Archive SHA-256:
`4f10c3424b86cd56c486cc75834b15b63b0fceedaa558be61afa165976a949a1`.
The extracted tree matched the source tree byte-for-byte, including executable
`4880b44`. Headful worker/font/complex-text checks: **10 passed**, 14.81s.
An initial font test returned an empty CDP glyph-usage list before layout;
the test now awaits font readiness and forces sample layout before querying
used fonts. The catalog test then passed ten consecutive runs without a binary
change. The initial failure is retained in the QA evidence.

The real API/proxy run from the extracted tree was **1 passed, 1 failed**:
one launch hit `ERR_TIMED_OUT`. Corresponding backend logs now report caller
context cancellation after approximately ten seconds, not a generator-budget
timeout. Archive integrity does not close this intermittent launch issue.
This manually assembled QA archive is not the signed release pipeline output.

## Tested artifact

- Linux x86_64, Ubuntu 26.04, headful under Xvfb, sandbox enabled.
- Chromium 151.0.7922.109; executable SHA-256
  `93b36fd816c00d14a9ae4c6be13ed53ff58cd40b450807f02d99831a38ab71bf`.
- Real candidate backend and authorized managed proxy, not cached fixtures.
- GeoLite2-City loaded read-only; database build June 12, 2026. This does not
  certify current city-level geolocation accuracy.

## New evidence

Actual backend integration tests: 2 passed. Headful Verify: 32 passed and 3
skipped, not 35 active passes. Skips concern native WebGL checks. Direct WebGL1
and WebGL2 contexts are available and expose coherent SwiftShader. A physical
GPU machine remains necessary to confirm physical-GPU isolation there.

AmIUnique observed the profile screen, matching HTTP/JS UA, and bundled fonts.
PixelScan fingerprint scan reported **Masking detected**. This remains unresolved;
neither a detector score nor this page alone proves the font isolation contract.

### PixelScan WebRTC checker: red verdict is not evidence of host-IP leakage

The real page at <https://pixelscan.net/webrtc-check> was opened via its observed
navigation link and the Start Check button was clicked. Screenshot inspection
confirmed a red verdict with empty local and external ICE/STUN/TURN fields.
Its displayed address did not match either address family of the QA host.

Network observation identified that same displayed address in the HTTP response
from `/s/api/wr`. Static inspection of the downloaded checker module explains
the verdict: its success flag requires the response's `publicIp` to occur in
the concatenated external IPv4/IPv6 STUN/TURN arrays. With all those arrays
empty, success is false and the UI renders “Potential Leak”. Thus absence of
candidates is also classified as a leak by this checker.

Checker module SHA-256:
`39152720f7308930a1db7e430f4cce8bb4c57bdb2689d04b7e4f78e42e8d4173`.
Downloaded vendor source is not redistributed here. This finding does not
justify synthesizing candidates or changing browser behavior to obtain a green
badge. Functional relay and host-egress blocking require separate controlled
network tests; those cannot be replaced with this screenshot.

### UA version investigation

The backend intentionally selects a full Chrome version from a same-major
catalog, rather than exposing the executable's exact build. The observed
151.0.787x.0 values are explained by `ResolveRuntimeBrowserVersion`, not by an
unexpected old executable. Captured UA, `uaFullVersion` and Chrome/Chromium
`fullVersionList` values agree. Focused backend version/header tests passed.
This does not establish the cause of the general Masking detected verdict.

## Startup deadline follow-up

The observed cancellation was preceded by proxy geo lookup, not evidence of a
15-second Python hang. Browser requests allowed only10s while proxy discovery
allowed15s. Backend PR3 now bounds generation end-to-end at30s and defaults its
HTTP write timeout to40s; explicitly overridden deployments need the same review.
Browser PR31 allows35s for generation and20s for verification. Portable nextctl
readiness allows60s; caller cancellation remains respected.

New Linux executable SHA256:
`c0e9cfad3f6ebe254be3ff24b4d781a9dc801e62833045c3e674835f7fe944c1`.
213 C++ tests passed. Two new deadline tests fail with the old10s implementation
and pass with the change. A loopback relay delaying real backend generation
responses11s produced two ERR_TIMED_OUT failures on4880b44 and two passes on
c0e9cfa. Ordinary real backend tests also passed2/2. This is a controlled response
delay, not a claim that an unreliable external proxy can never time out.
Headful surfaces passed45 with1 existing native-canvas-policy skip in56.00s.
QA backend image4427c2f is running; the previous container was retained. The
temporary delay relay is stopped. No production deployment or merge performed.

## Active-catalog packaging

The release copy function previously included stale font catalogs from an
incremental build output. It now reads the active catalog ID from catalog_build.json,
rejects unsafe IDs and missing active manifests, and copies only that directory.
Three packaging regressions failed before and passed after (macOS and Linux).
The rebuilt archive contains only prototype-3; the browser executable is unchanged.
Archive SHA256: `a4f6d98c9fd5e85912ac2158f122778c9aa0ac34cf2712628d3d08c7d5f4ca8b`.
Fresh extraction matched staged contents and passed both real API/proxy tests.
Seven headful font/complex-script/emoji tests passed in10.84s after extraction.
This executes the release staging functions, not the full ARM64/AppImage pipeline.

## PixelScan classification follow-up

Captured same-domain response diagnostics on the unchanged c0e9cfa binary.
PixelScan `/s/api/co` returns `comparedResult.match=true` (Linux) but
`osFontsStatus=false`. Its loaded public fingerprint component combines the
font result with other checks using logical AND. Thus this font classification
is a sufficient cause of the negative masking result, not proof of host-font
leakage. Other failing predicates have not been ruled out. The component's
hardware-memory allowlist includes both16 and32; do not clamp these to8 to
appease a presumed outdated detector. `/cbv` `legitimate=false` belongs to a
separate browser-classification path, not that masking conjunction.

Backend PR3 commit `ee7fff4` improves version coherence independently: prefer
known patch alternatives on the runtime build branch rather than early builds
of the same major. A regression failed with151.0.7874.0 before the fix. The
fingerprint/provider/app/API Go suites pass afterwards. Added7922 patch versions
were verified against the official Chrome-for-Testing catalog on September14.

A separate QA backend on loopback18081 produced151.0.7922.77, with matching
Window/Worker UA and Client Hints and passing internal Verify. PixelScan still
reports masking and the same negative font classification. This version change
does NOT resolve that verdict. Proxy geography varied between runs; this is not
a strict single-variable network experiment. Evidence directories:
`mask-diagnostic-c0e9cfa`, `mask-buildbranch-ee7fff4`, `mask-detail-ee7fff4`.
No browser privacy guard was disabled and no production service was changed.

The follow-up `mask-fontinput-ee7fff4` run captured the known font-check request
fields: `platform="Linux x86_64"`, `fonts=["DejaVu Sans"]`, `canvas=false`.
The canvas-test predicate is also required by the masking conjunction, so both
negative predicates need characterization. The exact failing canvas invariant
has not yet been identified. PixelScan probes a fixed candidate-font list;
this is not a complete enumeration of the bundled catalog or evidence of host
font exposure. The full backend `go test ./...` suite also passed.

## Canvas noise characterization and font-isolation recheck

The loaded PixelScan 2D test paints small solid-color tiles, exports PNG,
decodes it, and requires original channel values to remain exact. The new
`test_canvas_palette_roundtrip.py` distinguishes this requirement from unstable
readback or a broken PNG round trip, using native and protected controls on
the same c0e9cfa executable without visiting an external site.

Both controls passed: native pixels are unchanged; the protected fixture changes
554 channels by at most1. Alpha is unchanged, repeat readback/export are stable,
and the protected PNG round trip has zero mismatches. Both `toDataURL` methods
remain native with signature length38. The detector's exact-color requirement
conflicts with the intentional pixel protection for this input. This is not an
unfixed round-trip defect; it is also not a claim that every canvas path is safe.
No policy change to suppress protection on detector inputs was made.

The combined headful palette, GPU-upload snapshot, font-catalog and complex
fallback suite passed8/8 in11.32s. The two independent host font configurations
resolve monospace to Noto Sans Mono and Liberation Mono respectively, but the
protected text metrics are equal across them. This rechecks host-catalog
isolation; it does not imply PixelScan accepts the bundled font selection.
Evidence: `canvas-font-ab-c0e9cfa.xml` (including palette observations).
The actual QA backend `ee7fff4` and proxy tests also passed2/2 in6.727s on
this binary (`real-backend-ee7fff4-final.xml`).

## Remaining release gates

Fresh site run on the c0e9cfa active-catalog archive (September14): internal
Verify active checks passed with3 WebGL policy skips. AmIUnique and PixelScan
both loaded200; HTTP and JS UA agree, screen is2560x1440 versus Xvfb1440x1000.
PixelScan still reports masking/inconsistency. Its WebRTC checker labels proxy
IPv4 71.227.65.176 as Potential Leak while all ICE/STUN/TURN fields are empty;
the same address is returned by its HTTP /s/api/wr endpoint and matches proxy
verification, not the QA host IP. This is not a functional TURN test or an
all-clear detector result. These are operator integration tests, not desktop OAuth.

- Validate rollout timeout overrides and actual desktop startup on the new
  deadline contract; bounded proxy failures remain possible.
- Complete the real desktop app lifecycle/update-guard journey. Backend test
  credentials are not a substitute for actual desktop login acceptance.
- Resolve or explicitly characterize the general fingerprint detector result.
- Validate macOS/Windows and relevant physical GPU/display configurations.
- Produce and validate the actual release packages, not just this Linux binary.

Browser #30/#31, nextctl #26, backend #3 and app #235 must not be represented
as universally release-ready on the basis of these Linux checks.
