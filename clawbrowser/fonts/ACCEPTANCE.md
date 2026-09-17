# Font catalog acceptance boundary

`catalog.json` is a hashed runtime input. Its `release_gaps` list records the
prototype's original gaps; it is not a live acceptance report. Do not edit that
manifest merely to update progress: changing it changes the runtime identity
and requires rebuilding and retesting the resulting artifact.

## Established Linux coverage

The prototype-3 catalog has 23 families in 28 files (32 faces). The Linux
integration tests cover host-catalog isolation, profile migration, Verify
family detection, bundled glyph availability and selected visible fallback
samples. Test names and assertions, rather than the number of families, define
the coverage. A cmap hit is not evidence of correct shaping.

GN stages the catalog through `build_catalog.py`; startup validates it and
uses the isolated catalog. Relocated QA archives have been exercised. This is
not equivalent to validating signed installers for every target platform.

## Still required before release approval

- Specify supported languages, scripts and emoji sequences. Existing sample
  coverage does not establish complete Unicode or modern emoji support.
- Visually validate shaping, combining marks, bidirectional text and fallback
  on representative real pages for the supported language set.
- Preserve the pinned source hashes, original copyright records and packaged
  notices. Recheck notices in each final distribution, not only the source tree.
- Exercise each shipping installer and architecture, including missing/corrupt
  assets and upgrade from an existing profile. Linux Fontconfig results do not
  establish Windows/macOS behavior.
- Validate the real backend capability handshake with the shipping browser,
  nextctl and desktop application. Mock integration tests do not replace it.

`release_ready` remains false. `stage_fonts.py` deliberately accepts only this
explicit prototype state. A release transition requires a reviewed catalog
policy and corresponding staging changes, not simply flipping the boolean.
