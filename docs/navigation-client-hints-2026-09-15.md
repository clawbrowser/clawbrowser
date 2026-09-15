# Navigation Client Hints divergence

The managed Linux `63a0b953` ClawBrowser profile passes all 35 internal verify
checks, yet its PixelScan document request has a different `Sec-CH-UA` tuple
from `navigator.userAgentData`. The window and worker expose Chromium,
Google Chrome and `Not/A)Brand`; the navigation exposes Chromium and
`Not=A?Brand`. This is a real cross-surface inconsistency, not proof of the
cause of PixelScan's overall warning.

The new local-server regression `test_navigation_sec_ch_ua_headers` reproduces
the issue on the macOS `945d653` candidate: the fixture's major is 120 but
the navigation header exposes runtime major 151. Existing fetch-only header
tests did not exercise this browser-process path. After correcting an initial
test URL-construction error, the actual header assertion fails in 2.69s
(`navigation-ch-before.xml`).

Cause: `client_hints::ClientHints::GetUserAgentMetadata` calls
`embedder_support::GetUserAgentMetadata` directly. This bypasses the
ClawBrowser override in `ChromeContentBrowserClient`. Patch 024 now creates
a Chrome-layer delegate subclass that uses the same fingerprint metadata
builder and preserves downstream Client Hints permission rules. With no
fingerprint it calls the original delegate. The first implementation tried
to call `content::GetContentClient` from the components layer; Linux compilation
correctly rejected that content-private API. That attempt was removed rather
than bypassing the API boundary.
No header is injected through DevTools and no site-specific exception is added.

## Runtime evidence

The corrected factory implementation built successfully on Linux in 2m44s.
ELF SHA256: `a6f3c1aa11cfd72dc07c221f0d2f99efb2c9c7a2cf74cb4ee776f05c82deba7f`.
Archive SHA256: `1502f54d72aae1044824c33446ee25d41a2f0bba8be092f7fd60a4e87516ea0d`.
The extracted archive's ELF hash matches. Sandbox remains enabled.

- Old Linux navigation regression fails in 1.34s with major 151 instead of
  fixture major 120 (`navigation-ch-linux-before.xml`).
- Another old macOS regression fails in 2.70s: after server `Accept-CH` opt-in,
  full-version-list is empty, as is architecture. No client-side header
  modification is involved (`navigation-high-ch-before.xml`).
- New extracted Linux candidate passes both navigation regressions, existing
  fetch-header and JS metadata checks, and vanilla launch: 5 passed / 5.84s
  (`navigation-ch-linux-after.xml`).
- Normal managed restart with the new candidate passes all 35 internal checks.
  On PixelScan, Window/Worker metadata and the document's CDP-reported outgoing
  headers agree on brands and UA. This is a live managed browser observation;
  the local-server tests above independently observe received headers.

PixelScan still reports **inconsistent / Masking detected** on the new candidate.
The uninstrumented screenshot is `pixelscan-navigation-ch.png`. A subsequent,
separately labelled diagnostic reads already-computed classifier booleans with
a non-pausing conditional breakpoint: fonts=false, canvas=false; the other
11 conditions=true. It does not replace inputs or decisions. Debugging was
disabled after capture. This confirms remaining work, not a green website gate.

macOS rebuild remains blocked before compilation by Xcode's license acceptance
requirement; no license was accepted automatically. Full Linux headful
integration on this extracted candidate passed **120 tests, 16 skipped**, in
285.58s (`navigation-ch-linux-full.xml`), including two host-font controls.
The five targeted tests above are reported separately and overlap this suite;
do not sum them as distinct tests. No new-platform acceptance, complete leak
resolution, or merge readiness is claimed.
