# Linux privacy acceptance — 2026-09-14

Status: **Draft; not release approval.** No production deployment or merge.

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

## Remaining release gates

- Investigate the intermittent 15-second BrowserForge timeout. An unchanged
  retry passed; this is not a timeout fix or a reproduction of its root cause.
- Complete the real desktop app lifecycle/update-guard journey. Backend test
  credentials are not a substitute for actual desktop login acceptance.
- Resolve or explicitly characterize the general fingerprint detector result.
- Validate macOS/Windows and relevant physical GPU/display configurations.
- Produce and validate the actual release packages, not just this Linux binary.

Browser #30/#31, nextctl #26, backend #3 and app #235 must not be represented
as universally release-ready on the basis of these Linux checks.
