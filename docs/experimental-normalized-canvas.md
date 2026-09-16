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
`normalized-targeted-064.xml` in private QA evidence. Full regression, fresh
network acceptance with the experimental flag, the six-site comparison and
PixelScan are still pending. No macOS/Windows or green-detector claim.
