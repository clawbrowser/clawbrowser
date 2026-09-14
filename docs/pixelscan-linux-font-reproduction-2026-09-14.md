# PixelScan Linux font-classification reproduction

## Observation

On September 14, 2026, stock Google Chrome 153.0.8010.36 on Ubuntu 26.04 under
Xvfb returned **Masking detected** and **No automated behavior detected**.
`navigator.webdriver` was false. There were no ClawBrowser patches, fingerprint
overrides, JavaScript property replacements, or proxy in this reference run.
Chrome used a fresh temporary profile and its normal sandbox. CDP was used to
navigate and collect results; this was not an uninstrumented manual session.

PixelScan `/s/api/co` returned HTTP 200:

```json
{
  "status": "ok",
  "value": {
    "comparedResult": {"match": true, "sameGroup": false, "os": "linux"},
    "osFontsStatus": false
  }
}
```

The corresponding input had `platform="Linux x86_64"` and `canvas=true`.
The returned font result is a required predicate in the loaded frontend's
masking calculation. This reproduces the negative font result without the
ClawBrowser implementation. It does not establish that all detector warnings
are false positives, or that every Linux desktop behaves this way.

## Reference environment

Chrome was launched directly with a nonzero local debugging port, a new
user-data directory, `--no-first-run`, and `--no-default-browser-check`.
No `--enable-automation`, headless flag, no-sandbox flag, or stealth script was
used. The ordinary Playwright-launch control also returned the same font result,
but had webdriver=true and is not the primary reference above.

The reference font configuration included the server's installed fonts plus
Ubuntu archive packages extracted into a separate directory (not installed
system-wide):

- fonts-ubuntu 0.869+git20240321-0ubuntu2
- fonts-dejavu-core 2.37-8build1
- fonts-freefont-ttf 20211204+svn4273-4build1
- fonts-noto-cjk 1:20240730+repack1-1build1
- fonts-noto-color-emoji 2.051-1build1

The original, smaller system font set also produced osFontsStatus=false.
Adding fonts did not make this reference pass. No personal account was used.

## Questions for the detector maintainer

1. Which Linux/font-rendering invariant causes this response to be false?
2. Are current Ubuntu font versions and Chrome 153 covered by the classifier?
3. Can the UI distinguish an unfamiliar font signature from an actual
   inconsistency or detected modification?

The canvas font probe labels each distinct raster result with its first candidate
font name. Such labels are not necessarily successful local font lookups: in the
ClawBrowser control, Abyssinica SIL was reported while its local FontFace lookup
failed. Please clarify how fallback-equivalent candidate names are interpreted.

## Evidence and scope

Lab evidence directory: `stock-minimal-launch-full-fonts` contains the screenshot
and report captured before browser teardown. The process subsequently reported
a temporary-profile cleanup race; the recorded page and API results were already
saved. Do not count this as a clean teardown test. Network/IP observations in
this direct-network stock reference are not protected-product leak tests.

ClawBrowser's separate Linux Canvas2D raster-path fix passed 215 C++ tests and
54 headful tests (one existing skip), but does not make PixelScan green.
Its production canvas protection remains enabled. This report has not been
sent to an external party and is not a release or merge approval.
