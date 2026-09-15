# TTC variation clone regression

The first browser raster test failed on the previous `20260915-135316`
candidate: CSS `font-weight:100` and `font-variation-settings:"wght" 100`
rendered different CJK SC glyphs, with synthetic styles disabled. Returning
to the first style reproduced the first image, excluding a one-off repaint.

Two source defects were involved:

1. Blink's custom platform-data helper passed the default collection index 0
   when cloning a selected nonzero TTC face. Fontations rejects mismatched
   indices and silently returns the original typeface, ignoring the axes.
2. Even with an explicit matching index, Fontations' fused clone arguments
   omitted that index. Reconstructing the face therefore selected TTC face 0.

Patch 040 forwards the actual catalog face index through Blink's variation
and palette clone requests and preserves it in Fontations' fused arguments.
The default remains zero for existing non-catalog callers. No font selection
through the host manager or site-specific exception was introduced.

A C++ regression reproduced the lost CJK SC family before the Skia fix, then
passed on macOS and Linux: cloning to weight 675 retains both family and
weight. The browser regression on candidate `20260915-143518` passed for all
five CJK families (JP/KR/SC/TC/HK): weights 100, 400 and 900 produce distinct
images; equivalent explicit-axis images match each weight, including a repeat
control. Catalog provenance and complex-script/emoji tests also passed:
8 tests, 25.89 seconds. This is specific axis/raster evidence, not all possible
variation axes, palettes, shaping clusters or memory acceptance.

The patch pipeline now restores Skia-owned files through the nested checkout
instead of treating them as absent Chromium-root files. Both remote scripts
are covered by a temporary nested-repository regression; script tests pass.

Mac launcher SHA256:
`77a2462abe5acbabc4e9c7a86abe40a47380a5eab06b4e0ae0fdbce1d06949b1`.
Mac framework SHA256:
`4a5b69c0cfc84f2de6492077310d61372bb26c5981f7f772c87b833c57d7509a`.
The complete macOS integration run on this candidate finished with 118 passed,
2 failed and 14 skipped in 350.15 seconds. Both failures remain the native
canvas equality controls; protected-mode controls passed. In the native
color-format matrix, DOM/OffscreenCanvas/worker hashes agree within all 16
identical-setting groups, but changing `willReadFrequently` changes all eight
color-space/type/read-format combinations. This narrows the observed failure
to raster-path consistency, not a TTC-clone failure. The assertions remain
unchanged; native canvas is not certified hardware-independent.

Core C++ suites passed: 218 on macOS, 217 on Linux. The Linux chrome build
completed with ELF SHA256
`0631a217b3795916735477719f7ae6e37daab8690ddf4057fc4dda36fa9ef82c`.
The fresh archive SHA256 is
`47469cda291a55ced1babb264cbf8709f18237dac4054f6b2b2403a5b376e7ed`;
the independently extracted executable matches the build hash. The extracted
archive passed the complete headful Linux integration run with two isolated
host Fontconfig configurations: 119 passed, 16 skipped, zero failures in
267.72 seconds. This includes proxy-disconnection fail-closed and implicit
loopback/link-local bypass regressions. The 16 skips are not passes; external
network tests and macOS-only tests have separate prerequisites. Network
acceptance was then rerun against this newer binary:

- Controlled IPv4/IPv6 STUN packet capture: independent controls produced four
  records; HTTP/SOCKS5 browser probes produced zero direct records and zero
  candidates for both families. Both captures reported zero kernel drops.
- Real TURN/TLS DataChannel echo: both HTTP and SOCKS5 cases passed in 19.82
  seconds. Both peers selected successful relay-to-relay pairs with TLS and
  positive sent/received bytes. No forced relay policy in the test page, TLS
  certificate bypass, or disabled browser sandbox was used.
- Bounded TURN/STUN services are stopped after the tests. Raw packet captures
  remain private; sanitized JSON and JUnit reports are the shareable evidence.

These controlled network results do not substitute for managed PixelScan or
desktop OAuth/update acceptance. No release or production deployment occurred.

## Bounded memory observation

The opt-in `test_macos_catalog_memory.py` repeats a fixed 21-combination font
working set after warmup (five batches of 20 cycles, 2,100 additional draws),
collects garbage, and verifies the renderer PID set did not change. On this
candidate it passed in 3.24 seconds. Combined RSS of four renderer processes
rose from 622,804,992 to 624,607,232 bytes (about 1.72 MiB). RSS includes shared
pages; this is neither unique memory accounting nor a production memory budget.
It catches large repeated-load growth, not arbitrary long-running leaks.
The immutable catalog still retains roughly 50 MB of font bytes in each
process that initializes it, plus transient validation copies. A shared-memory
catalog transport could reduce that cost but is not implemented or claimed.
