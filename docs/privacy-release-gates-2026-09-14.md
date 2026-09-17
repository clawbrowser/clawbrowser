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

- A macOS build of the actual candidate source. The configured remote builder
  presents a changed SSH host key; do not bypass host verification. An isolated
  local arm64 build is now in progress at pinned Chromium revision
  `28a7a6c409e03c701d3474ef9e3b1f0be6249039`, with SDK 26.5 and working Metal.
  Preparation and compilation progress are not a completed binary or test pass.
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

## Portable corpus preparation

The canvas corpus now has a separately named native-host entry point, because
the existing two-fontconfig test deliberately skips on non-Linux systems.
Both use the exact same rendering recipe and cross-context/hash assertions.
The native-host gate records its platform, scope and single host-font environment;
it must not be represented as a two-font isolation result.

Regression on the Linux candidate above: **5 passed, 0 skipped, 144.45 seconds**.
This includes the original three checks plus two native-host modes. The original
cross-context matrix retains 240 pixel observations; the native-host entry adds
120 overlapping observations on the same machine, not another hardware sample.
JUnit artifact: `canvas-native-host-portability.xml`. Its properties were parsed
and verified despite pytest's xunit2 compatibility warning. macOS execution of
this corpus remains pending the new binary.

The portable blocked-family fallback test also passed on that Linux candidate
(1 test, 1.74 seconds). It compares 15 requested-family/text pairs with the
generic-family control, including Arabic, CJK, Devanagari and joined emoji;
records actual CDP platform-font provenance; and requires matching glyph fonts
and widths. This is not proof of independence from the OS fallback catalog, nor
does a family absent on the host provide a positive installed-font control.
Its macOS run is still pending. Artifact: `portable-font-fallback.xml`.
