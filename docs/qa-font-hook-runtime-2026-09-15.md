# macOS closed catalog and cross-platform regression evidence

Status: experimental QA, all related PRs remain draft. No production deployment.

| Gate | Actual result |
| --- | --- |
| macOS source/build | FontCache family, character and last-resort paths compiled; catalog packaged inside signed framework Resources |
| macOS C++ | 218 passed, including missing/corrupt catalog startup rejection |
| macOS headful font tests | 8 passed: complex scripts, composed emoji, catalog provenance, enumeration/local-name/worker restrictions and verify contract |
| macOS full headful | 113 passed, 2 failed, 14 skipped, 241.89s |
| macOS failures | Existing native canvas cross-context equality controls; protected-mode controls passed |
| Linux C++ | 217 passed as builder, not root |
| Linux fresh extracted archive | 119 passed, 10 skipped, 269.95s; two isolated Fontconfig controls and browser sandbox enabled |
| Linux STUN capture | Independent IPv4/IPv6 positive control: 4 records; HTTP/SOCKS5 browser: 0 direct UDP records and zero candidates, zero capture drops |
| Linux TURN/TLS | 2 passed, 18.59s; HTTP and SOCKS5, actual DataChannel echo, relay-only successful pairs and positive byte counts |
| Backend catalog contract | PR3 accepts exact catalog capability on Linux/macOS; Windows remains rejected. `go test ./...` passed |
| Real managed PixelScan | Blocked before CDP: live backend header generator rejects runtime request. Not a site acceptance pass |

## Artifact identity

- Product overlay: PR31 `dfd76ea`; follow-up tests: `988640e`.
- Backend PR3: `a6e43ec`.
- Pinned Chromium: 151.0.7922.109, `28a7a6c409e03c701d3474ef9e3b1f0be6249039`.
- Mac staged bundle: `20260915-135316/Clawbrowser.app`.
- Mac launcher SHA256: `a230b3dd6e0a92b6abc1de8d0939bc5e3c4bb20d35cebd32242b1e276f9b01bb`.
- Mac framework SHA256: `487253d6b7054b99c7bffbbb1ede17adbef7fbe3de748ec53e09ff66edc69cce`.
- Linux ELF SHA256: `37612aec891055e2e50f1035169e2fab96b0fa9f631d6a2f317f358e58b3be06`.
- Linux archive SHA256: `eec9781b871d4671eae5eb9349eb07fd02c1312ae3fade4f799cf69c55c1452f`.
- Catalog manifest SHA256: `7997598edceb7eab61bdbab53fa1ee923770ab349c4b8a5034c16b5dc540bbc7`.

Sanitized XML/JSON and the macOS rendered fixture screenshot are retained in
the operator workspace. The screenshot is not a PixelScan screenshot. Private
proxy/TURN configs and packet captures are not committed. Bounded TURN, STUN
and PixelScan display services were stopped and verified inactive.

## Failures and scope that must not be hidden

The first macOS full run had four additional failures because surface tests
still expected legacy macOS fonts. Expectations now explicitly name the new
catalog, independently of browser-reported JSON. The renderer provenance test
and local-name migration tests separately verify that it actually loads.
The initial standalone C++ run lacked a framework resource bundle; unit tests
now provide an isolated copy of real catalog assets, without changing product
validation. Failed reports are retained.

The live managed account has available proxy traffic, but its fingerprint
generation reports `No headers based on this input can be generated`. The new
browser exits rather than continuing unproxied. The skill metadata endpoint
also reports invalid JWT; it is separate from proxy-traffic authentication.
Do not repeat identical starts, substitute a fixture as managed acceptance,
or deploy production merely to bypass this gate.

Still required: real managed PixelScan after backend access/deployment is
resolved, actual desktop runtime-update installation/defer lifecycle, macOS
catalog memory/variation-axis review, and remaining platform builds. The
STUN proof covers only the controlled endpoints, not arbitrary network paths.
