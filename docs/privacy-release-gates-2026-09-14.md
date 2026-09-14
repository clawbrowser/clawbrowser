# Privacy release gates — September 14

This is a release decision record, not approval to merge or publish.

## What the current evidence supports

The extracted Linux x64 candidate executable SHA256 is
`a98431433f88fdd5152e8cadb50c1887ab12c0f84825575e24571d63ea6bc62a`.
See [Linux acceptance evidence](linux-privacy-acceptance-2026-09-14.md) for
test configurations, positive controls, skips and overlapping counts.

| Concern | Current evidence | Remaining boundary |
| --- | --- | --- |
| Direct WebRTC routes with HTTP/SOCKS5 proxies | Controlled IPv4/IPv6 STUN: no direct candidates or browser-phase UDP packets; separate positive capture works | Scoped endpoints/protocol, not proof for every possible network path |
| Usable proxied WebRTC | Real TURN/TLS DataChannel echo, selected relay pairs and positive byte counts | Other OS/runtime packages still need execution |
| Host font leakage | Bundled catalog isolation, glyph/fallback and cross-context regressions | Detector font classification is not equivalent to font enumeration |
| Screen/WebGL/profile coherence | Candidate integration and QA-backend checks | Physical GPU/display and other-platform coverage incomplete |
| Canvas consistency | Seeded corpus, Window/Worker, P3/float16, PNG/alpha and origin-clean checks | One physical Linux x64 host; intentional noise remains detectable |
| App lifecycle | Packaged macOS UI start/stop, polling-race fix and Live View video/input retest | Installed runtime, not this Linux binary; real update installation/defer not exercised |

## Why PixelScan is not green

The diagnostic identified two negative predicates, fonts and canvas. Other
observed predicates passed; that is not an exhaustive detector guarantee.

1. **Canvas:** the checker compares known colors against exact expected values.
   The current protection intentionally changes low-order RGB bits. Stable,
   internally consistent noise can still be detected by that comparison.
2. **Fonts:** stock Chrome controls also returned a negative font classifier
   result on this Linux host. This prevents attributing the result solely to
   our patch, but does not establish that every warning is a false positive.

See the [reproduction report](pixelscan-linux-font-reproduction-2026-09-14.md).
No site-specific exception, spoofed test result, or default protection disable
is an acceptable substitute for a verified engineering fix.

## Next evidence needed

- A verified macOS builder identity and a build of the actual candidate source.
  Its configured SSH endpoint currently presents a changed host key. Confirm
  through the owner/provider console; do not bypass host verification.
- A second physical rendering environment for the normalized-rendering
  experiment. Compare the same native-policy corpus and font catalog against
  the Linux baseline before considering replacement of per-profile noise.
  Agreement would support further investigation, not automatically approve a
  default-policy change or establish Windows/ARM equivalence.
- For the font classifier, either a reproducible supported-environment control
  that distinguishes the relevant invariant or clarification from the detector
  maintainer. A report is prepared, but has not been sent externally.
- Execute the real update-install/defer journey with an appropriate candidate
  runtime available; checking an already-current runtime does not cover it.

## PR dependency and disposition

Browser #30 targets main; browser #31 is stacked on #30. Backend #3, nextctl #26
and app #235 contain corresponding contracts/lifecycle changes. Green CI and
conflict-free branches are necessary but insufficient for blanket readiness.
Keep these gates visible when handing the work to the platform build team.
Do not label the full issue set fixed, approve all-platform release, or equate
the separately tested macOS Electron package with a tested macOS Chromium build.
