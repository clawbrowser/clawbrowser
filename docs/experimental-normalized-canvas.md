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

Next: investigate the remaining font classifier and the independently observed
version discrepancy. Preserve network guards and the current default until
cross-host/platform rendering and the identity-versioning contract are
established. No macOS/Windows acceptance, default-policy change or release
approval. A website score never replaces controlled network gates.
