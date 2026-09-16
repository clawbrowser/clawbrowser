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
