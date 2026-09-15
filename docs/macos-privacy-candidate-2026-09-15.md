# macOS candidate: build success, acceptance blocked

The local arm64 build of Chromium 151.0.7922.109 at
`28a7a6c409e03c701d3474ef9e3b1f0be6249039`, with the candidate overlay and
patches through 037, completed all 57,990 Ninja actions. The staged QA app is
ad-hoc signed, not an installed or released product. Its launcher SHA-256 is
`7d33caded59b4f8e741db94a7127f8436188ee19a498f7c187e698d90ba9c719`;
this identifies the launcher, not the complete framework payload.

## Results

- C++: 214 passed, zero failures.
- Selected headful integration: 58 passed, 5 failed, 2 skipped in 195.38s.
- Skips: native canvas policy fixture and unconfigured external STUN endpoint.
- Portable blocked-family fallback provenance passed. This is not proof that
  macOS uses the Linux bundled catalog; it does not.
- Four failures: native/override drawing corpus and color-format matrix.
  DOM, OffscreenCanvas and Worker agree within each `willReadFrequently`
  setting, but false and true produce different hashes. Disabling Skia runtime
  optimizations did not remove the difference.
- Diagnostic-only `--disable-accelerated-2d-canvas`, with all original assertions
  and protection retained: all four matrices passed in 40.04s. This isolates the
  accelerated/software rendering path distinction; it is not a product fix or
  permission to disable GPU globally.
- The fifth failure was the expected-font test's width heuristic. Courier New
  was not distinguished from generic monospace. Replacing that heuristic with
  actual CDP glyph-family checks passed for every expected family in 1.90s.
  Linux's existing bundled-catalog provenance assertions are unchanged.

Evidence is retained in the local QA task's `evidence/20260915-054018`:
`cpp.xml`, `candidate-gates.xml`, `software-canvas-diagnostic.xml`, and
`font-provenance.xml`. The original failing results are preserved.

## Remaining gates

Choose and validate a general, policy-scoped normalization strategy for canvas
rendering, including its performance impact. Do not add detector-specific
exceptions or weaken assertions to mask the path difference. Repeat the full
suite against any changed binary, and complete managed identity, real network
and platform gates. PixelScan's Linux fonts/canvas conditions remain unresolved.
These results supersede the older documents' “macOS binary pending” status,
but do not make the PRs ready for merge or establish Windows/Linux ARM support.
