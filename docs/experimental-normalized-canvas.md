# Experimental normalized Canvas (Linux QA only)

This opt-in prototype is not the default and is not an accepted replacement for
seeded pixel protection. It does not claim cross-platform anonymity or a green
fingerprint-checker result.

Use `--clawbrowser-experimental-normalized-canvas` only with a managed profile
whose Canvas policy is `override` and whose nonempty font catalog policy is
`native_or_allowlist`. The ordinary Canvas enable/disable resolution still
applies: an explicit `--disable-canvas-spoofing` cannot activate this experiment.
Unsupported platforms and ineligible policies retain their ordinary behavior.

The browser and child loaders resolve a process-local experimental bit. It is
not written to the cached fingerprint and does not change a backend schema,
profile seed, default launch or API response. A new launch without the flag
therefore returns to the ordinary policy. Use separate durable QA profiles for
the experiment and the existing protected baseline.

The experiment skips pixel perturbation in ImageData reads and Canvas exports.
It deliberately keeps `canvas_spoofing_enabled` and Canvas `override` policy
active, retaining protected software-raster and text-transfer branches. Canvas
source snapshots still go through the checked copy/encoding path, preserve
origin-clean status and fail on allocation/copy errors. Fonts, WebGL,
fingerprint-required startup, proxy enforcement and WebRTC routing are unchanged.
This is not a shortcut to launching an unconfigured native browser.

Canvas seeds no longer distinguish otherwise identically rendered canvases in
this mode. Other fingerprint surfaces are not unified by this change. Before a
production rollout, the cohort identity and migration/versioning contract must
be specified and verified across real platforms and hardware.

## Validation status

The new palette test against the old 063 archive produced 2 passes and the
expected normalized-case failure: 554 channels still changed. That establishes
that an ignored flag does not satisfy the test. It is not proof of the new
binary. New-build palette, child/worker behavior, isolated-font corpus, source
transfers, origin-clean security, network guards and site comparisons remain
required before accepting the experiment.

The current default must still perturb pixels; the test keeps that assertion.
No site, canvas dimension, color, font-probe name or detector verdict is
special-cased.

### First Linux artifact results, September 16, 2026

- Candidate: `normalized-experiment-064`, code `ac09fde` (subsequent color-format
  test additions do not change the binary).
- ELF SHA-256: `3c2930940919aaf4a45849bd69345173191e828585831bbf8798132fb2164a0a`.
- Archive SHA-256: `3353d73b74fc969b322482bc01d8f5e7c75d4db657e2ac8e4a35abafc93583e4`.
- Selected C++ startup/child-loader/noise tests: 92 passed in 3.435s.
- Extracted-archive browser tests: 15 passed, zero skips, in 93.10s; non-root,
  headful and sandbox enabled with scoped user-namespace permission.

The browser run covers original/protected/normalized palette controls,
protected and normalized GPU/source snapshots (including bitmaprenderer),
three-policy origin-taint controls, sRGB/Display-P3 and unorm8/float16 readback
and PNG controls. The normalized seeded drawing corpus also matches across
Window, OffscreenCanvas and Worker, two independent host-font configurations,
and normal/disabled Skia runtime optimizations on this single Linux host.
The old 063 normalized palette failure is now a pass; the protected control
still requires nonzero perturbation.

Reports: `normalized-064-units.xml`, `normalized-before-063.xml`, and
`normalized-targeted-064.xml` in private QA evidence.

The complete Linux integration run finished with 177 passed and 21 skipped in
538.983s (`full-suite-064.xml`). Skipped cases are not counted as acceptance.

Fresh network runs tested this exact extracted artifact in both protected and
normalized modes. Every live browser phase includes an exact-color Canvas
canary: protected mode changed 368 channels by at most one, normalized mode
changed zero. Thus an ignored experimental flag cannot satisfy these results.

| Controlled check | Protected | Normalized |
| --- | --- | --- |
| IPv4/IPv6 STUN, HTTP and SOCKS5 | 0 direct packets | 0 direct packets |
| Independent STUN positive control | 4 packets | 4 packets |
| TURN/TLS echo, HTTP and SOCKS5 | 2 passed | 2 passed |
| Browser direct TURN packets | 0 | 0 |
| Proxy-origin TURN packets | 257 | 248 |
| Direct TLS positive control | 7 packets | 7 packets |
| Packet capture drops | 0 | 0 |
| Invalid profile/API/proxy startup paths | 5 passed | 5 passed |
| Proxy 407 / 502 / offline direct requests | 0 / 0 / 0 | 0 / 0 / 0 |
| Deliberately direct outage control | 35 detected | 35 detected |

TURN used trusted TLS without certificate bypass; both selected candidate pairs
were relay-to-relay with real DataChannel echo. STUN/TURN browser phases were
non-root, headful and sandboxed. The separate nextctl origin-side outage test
uses headless mode, `--no-sandbox` and a controlled test-certificate bypass;
these limitations do not apply to the TURN run. Network evidence is limited
to these controlled endpoints, not every possible destination/protocol.

Evidence stems: `normalized-064-{protected,experimental}-stun-pcap.json`,
`-turn-route.json`, `-turn-route.xml`, and `-outage.jsonl`. No proxy credentials,
raw SDP or private packet captures are published.

### Initial live-site observations

Both modes were launched as managed, non-root headful ClawBrowser profiles on
064, with 35/35 internal verification checks passing. The separate durable
profiles have different identities/proxy exits and UA patch versions; this is
not a same-profile randomized A/B experiment. Controlled fixture tests above
isolate the Canvas-mode difference.

- PixelScan: ordinary, settled scans in **both** modes report Inconsistent /
  Masking detected, No proxy detected and No automated behavior. Screenshots
  are preserved as `pixelscan-normalized-064-{protected,experimental}.png`.
- A subsequent, explicitly instrumented diagnostic in the normalized profile
  observed `canvas=true`, `fonts=false`, and the other eleven classifier
  predicates true. It reads the existing booleans without replacing their
  inputs or verdict; it is diagnosis, not a clean acceptance run. The old 063
  diagnostic had both Canvas and fonts false. The remaining font classification
  is unresolved, not assumed to be a false positive.
- CreepJS: the protected profile reports `14% rgba noise`; the normalized
  profile has no RGBA-noise label. Both show blocked host/STUN connections,
  `0% headless`, `0% stealth`, and `50% like headless`. This is not an overall
  undetectability verdict. Both expose the same observed font subset.
- BrowserScan: **95% authenticity in both modes**, with a five-point deduction
  for a different browser version. Both show Proxy No, Bot Detection No, and
  WebRTC/STUN disabled. The version warning needs separate diagnosis; removing
  Canvas perturbation did not resolve it. The displayed DNS results have not
  yet received origin-side routing acceptance and are not labeled leak-free.

BrowserScan follow-up on the same normalized 064 managed profile: its separate
`/browser-checker` page detects Chrome 151 and explicitly reports that the
browser version and User Agent match. Saved evidence is
`checker-browserscan-kernel-064-experimental.{json,png}`. Runtime CDP reports
151.0.7922.109, while page UA and high-entropy Client Hints consistently report
151.0.7922.71. CDP's runtime version is an operator diagnostic, not evidence that
the website can read that exact patch. The homepage's five-point deduction
therefore cannot yet be attributed to an inconsistent UA/Client-Hints pair or
an incorrect major version. The homepage classification consumes a separate
decision involving header UA, JS UA, Client Hints and experimental probes.
Its remaining warning is unresolved; neither overriding the site verdict nor
changing the identity speculatively is an acceptance fix.

Follow-up: backend PR3 `9d6a6ef` corrected the modern Chromium UA reduction
contract (major.0.0.0 in UA; full version in Client Hints). A fresh 064 profile
against that QA backend passed 35/35 internal checks, Window/Worker/HTTP identity
comparison, and a settled BrowserScan page reported 100%. PixelScan still
reported Inconsistent / Masking. The fresh profile is not a pure website A/B
experiment because its identity and proxy exit differ.

Runtime `2bc7857` also projects coherent legacy cached Chrome UAs into that
reduced form in memory, identically in browser and child loaders. It does not
regenerate or rewrite the cached envelope, change full Client Hints, seeds,
or proxy settings. Its new loader regression failed in both processes before
the change; 83 loader/startup unit tests passed afterwards. On the extracted
`cached-ua-065` Linux artifact, all seven privacy-migration tests passed headful
with sandbox (8.63s). The new Window/Worker/no-regeneration case failed on the
old 064 artifact, providing a negative control. This is targeted acceptance,
not a rerun of the full 064 network/canvas matrix or macOS/Windows acceptance.

065 ELF SHA-256: `767337994307176a03117a32b234277d59bd45ef66245445cb0180919c2305a4`.
Archive SHA-256: `2809e03b0616801b84e33b8b6f17991c517c03641523cbbc52a023e0fbf63954`.
Evidence: `cached-ua-065.xml`, `checker-browserscan-064-reduced-ua-settled.{json,png}`,
and the backend PR3 `docs/ua-reduction-qa.md` report. No release was published.

The subsequent complete headful 065 suite passed **185 tests with 21 skips**
in 528.59s (`full-suite-065-recheck.xml`). The first run had 183 passes and two
old full-UA expectations in profile-isolation/implicit-selection tests; both
were corrected to the reduced-UA contract without removing their isolation
assertions, and the complete suite was rerun. Skips remain unaccepted coverage.

A further font-probe regression passed in protected and normalized Canvas modes
(two tests, 2.66s): three unavailable family names, three generic families and
three multilingual strings produced 54 complete-pixel/metric comparisons equal
to their generic fallbacks. `FontFace(local)` failed for each unavailable name;
nonempty ink, positive metrics and an active Canvas-mode canary were required.
This supports the distinction between a requested font label and actual local
font availability, not a blanket dismissal of PixelScan's remaining font result.
Evidence: `font-probe-fallback-065.xml`.

Fresh 065 controlled network checks also passed in both Canvas modes:
HTTP/SOCKS5 over IPv4/IPv6 STUN produced zero direct browser packets with four
independent positive-control packets per mode. TURN/TLS produced two successful
relay-to-relay DataChannel echoes per mode, verified the TLS certificate, and
recorded zero direct browser packets against seven direct-control packets.
Proxy-origin packets were 252 (protected) / 256 (normalized); all captures had
zero kernel drops. These were headful, non-root, sandboxed runs. Evidence stems
are `cached-ua-065-{protected,experimental}-{stun-pcap,turn-route}` (JSON and TURN
JUnit XML). The controlled services were stopped after testing. The separate
outage/fail-closed gate was subsequently run fresh on 065 in both modes.
All five startup rejection cases passed: corrupt profile, missing host,
missing port, unsupported scheme and unavailable API. HTTP/HTTPS/WS/WSS probes
under proxy 407, 502 and offline conditions produced zero origin/direct
requests. Attempt counts were 20/20/20 (protected) and 18/22/20 (normalized).
Independent direct-browser controls detected 34/35 requests respectively.
Reports: `cached-ua-065-{protected,experimental}-outage.jsonl`. Unlike the
TURN/TLS runs, these controlled outage tests run headless with no-sandbox and
test certificate bypass; they are not represented as sandbox/TLS validation.

### Completed seven-site first pass

These are observations on September 16, not timeless detector guarantees.

| Site | Protected default | Normalized experiment |
| --- | --- | --- |
| [PixelScan](https://pixelscan.net/fingerprint-check) | Inconsistent / Masking | Inconsistent / Masking; diagnostic Canvas passes, fonts fail |
| [CreepJS](https://abrahamjuliot.github.io/creepjs/) | 14% RGBA-noise label | No RGBA-noise label |
| [BrowserScan](https://www.browserscan.net/) | 95%; browser-version deduction | 95%; same deduction |
| [IPhey](https://iphey.com/) | Trustworthy, MX 100; WebRTC IP null | Trustworthy, MX 100; WebRTC IP null |
| [BrowserLeaks Canvas](https://browserleaks.com/canvas) | 100% unique signature | 100% unique signature |
| [AmIUnique](https://amiunique.org/fingerprint) | Unique among 5,519,960 fingerprints | Unique among 5,519,964 fingerprints |
| [EFF Cover Your Tracks](https://coveryourtracks.eff.org/) | Unique; no tracking-ad/invisible-tracker blocking | Unique; no tracking-ad/invisible-tracker blocking |

EFF estimated at least 18.28 identifying bits for each profile, against
318,766 protected-run / 318,764 normalized-run observations in its past-45-day
dataset. This product has not gained tracker blocking from a rendering change.
Uniqueness is a different criterion from coherent spoofed surfaces or absence
of real-IP leaks. These results do not support a claim of passing a majority
of interchangeable tests: the tests are not interchangeable.

Local evidence pairs are `checker-{creepjs,browserscan,iphey,browserleaks-canvas,
amiunique,eff}-064-{protected,experimental}.{json,png}`. Each JSON preserves the
ordinary page text and each PNG the full page. PixelScan clean evidence is
listed above; its separate diagnostic must not be passed off as a clean scan.
No accounts, external messages, purchases, cookie-consent acceptance or
classifier exceptions were used.

### Origin hostname routing (065, September 16)

`test_proxy_hostname_routing.py` passed **6 headful tests in 10.40s** on the
065 artifact, as non-root `builder` with sandbox enabled. HTTP, SOCKS5 and
authenticated SOCKS5 each delivered the original hostname to the controlled
proxy, both with normal resolution and with that hostname mapped to
`~NOTFOUND` in Chromium's resolver. Each run uses a fresh `.invalid` hostname
and checks an actual page response, not only a successful launch.

A concurrent `tcpdump -i any` observation of port 53 recorded **2 observations
of the independent `dig` control, 0 observations of the test origin names,
and 0 kernel drops**. System resolver/network configuration was unchanged.
Evidence: `dns-route-065.json` and `dns-route-065.xml`; raw DNS output remains
private. This run used the default protected Canvas policy.

Scope is ordinary HTTP destination-name routing via an IP-literal proxy.
It does **not** accept DoH, DoT, proxy-host bootstrap resolution, arbitrary-page
DNS-prefetch, HTTPS destination-name routing, or other platforms. In the
authenticated bridge source, `ConnectTcp` resolves the upstream proxy host;
`AppendSocksAddress` sends origin domain names using SOCKS address type 3.

The first 3-case run had one test-harness failure: a startup request to an IP
literal was incorrectly required to use SOCKS domain addressing. The harness
now accepts valid literal addressing while requiring the test origin's exact
domain at the proxy. No runtime code was changed for this result.

### Page-initiated DNS hints (065)

`test_proxy_dns_hints.py` supplies opt-in controlled `dns-prefetch` and
`preconnect` stimuli. Its pytest result alone is **not** a DNS acceptance gate.
The operator runner `scripts/qa_dns_hints_linux065.py` additionally requires
both hints in an unproxied ClawBrowser control to produce observable DNS traffic,
zero corresponding observations with HTTP/SOCKS5/authenticated SOCKS5, and zero
capture drops. Fresh random names under `example.com` distinguish the cases.
The page is controlled; no live accounts or user profiles are involved.

The runner is scoped to the existing Linux QA layout under `/opt/clawbrowser-qa`,
the pinned 065 artifact, non-root `builder` browser processes, and `xvfb-run`.
Run it as the QA operator with tcpdump privileges, not on a production host.
It refuses existing evidence paths and stores raw DNS output in a mode-0700
private directory. It does not change resolver configuration or firewall rules.
Use `QA_DNS_HINT_LABEL=dns-hints-065-protected-15s` or
`dns-hints-065-normalized-15s`, `CLAWBROWSER_DNS_HINT_SECONDS=15`, and the
corresponding `CLAWBROWSER_QA_NORMALIZED_CANVAS=0` or `1`. The test asserts an
active Canvas-mode canary for each managed case.

The initial four-second observation passed all four stimulus cases in 22.61s:
28 port-53 observations for each unproxied hint, zero for every proxied hint,
and zero capture drops (`dns-hints-065.{json,xml}`). These are packet-text
observations, including requests/responses and loopback, not unique DNS queries.
The extended protected run passed four cases in 67.08s with 15 seconds per case
and the same 28/28 direct, zero proxied, zero-drop result.
The normalized-Canvas run also passed four cases in 67.48s, with 15 seconds
per case and the same counts. Evidence:
`dns-hints-065-{protected,normalized}-15s.{json,xml}`. Thus both Canvas policies
passed this bounded DNS-hint gate; this does not extend its DNS or platform scope.

This checks HTTP-document hints with IP-literal proxies and normal system DNS.
It does not accept HTTPS documents, DoH/DoT, proxy-host bootstrap DNS, or an
unbounded observation window. Chromium's `PreconnectManagerImpl::TryToLaunchPreresolveJobs`
looks up proxy configuration before issuing DNS; `OnProxyLookupFinished` skips
local preresolution when a proxy exists. Runtime code was not changed here.

### HTTPS hostname routing (065)

`test_proxy_tls_hostname.py` passed six headful, non-root/sandbox cases per
Canvas policy: **protected 10.99s, normalized 11.04s**. HTTP CONNECT, SOCKS5
and authenticated SOCKS5 each delivered the exact original hostname and port
to the proxy. A controlled TLS server returned an actual HTTPS page, with
TLS 1.2/1.3 recorded at the server, both with normal resolution and with the
origin mapped to `~NOTFOUND`. The active Canvas canary was checked in each case.

Each test generates a fresh random hostname under `example.com`, certificate
and key. Using this suffix avoids relying on a resolver's special handling
of `.invalid`. A separate Python client verifies the test certificate and its
hostname and receives a live TLS response. **The browser uses a narrowly scoped
SPKI exception for that temporary certificate**, not public-CA trust and not a
global `--ignore-certificate-errors`. This is a routing gate, not acceptance of
the browser's certificate validation policy. Temporary keys are never published.

The port-53 observer required a successful independent `dig` control, all six
JUnit cases actually executed without failures/skips, and zero capture loss.
Both runs recorded **4 control DNS observations, 0 origin-name observations,
0 kernel drops**. Reports: `tls-dns-065-{protected,normalized}-example.{json,xml}`.
The previous `.invalid` exploratory runs also passed but are not the final
ordinary-suffix acceptance evidence.

The QA-layout-specific operator runner is `scripts/qa_tls_dns_linux065.py`.
Use `QA_TLS_DNS_LABEL=tls-dns-065-protected-example` or
`tls-dns-065-normalized-example` and the corresponding
`CLAWBROWSER_QA_NORMALIZED_CANVAS=0` or `1`. Its private capture directory and
reports must not already exist. Raw DNS stays in the private mode-0700 QA
directory; only aggregate results are copied out. It does not change network,
resolver or firewall configuration. Runtime code was not changed for this gate.

Remaining DNS scope: DoH/DoT, proxy-host bootstrap, HTTPS-page prefetch hints,
other platforms and unbounded observation windows are not accepted by this test.

Next: investigate the remaining font classifier and the uncovered DNS scopes.
The reduced-UA BrowserScan result is recorded earlier in this report.
Preserve network guards and the current default until
cross-host/platform rendering and the identity-versioning contract are
established. No macOS/Windows acceptance, default-policy change or release
approval. A website score never replaces controlled network gates.
