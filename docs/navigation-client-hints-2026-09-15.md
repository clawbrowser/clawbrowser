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
ClawBrowser override in `ChromeContentBrowserClient`. Patch 024 now routes
the delegate through the content browser client, preserving the existing
fingerprint metadata builder and downstream Client Hints permission rules.
No header is injected through DevTools and no site-specific exception is added.

Validation in progress: patch syntax/reverse application and fingerprint
contract pass. Linux incremental runtime build is pending. macOS rebuild is
blocked before compilation by Xcode's license acceptance requirement; no
license was accepted automatically. This fix is not yet runtime-validated
and does not make the PR ready to merge.
