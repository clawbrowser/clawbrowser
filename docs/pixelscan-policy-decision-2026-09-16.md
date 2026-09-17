# Remaining PixelScan policy decision

Status: proposal only. No default, browser flag, backend policy, or protection
has been changed by this document. All related pull requests remain draft.

## What is known

Linux candidate `metric-overrides-063` has a clean negative PixelScan result.
A separate instrumented trace on September 16 at 17:43 UTC isolates negative
font and canvas predicates; eleven other evaluated predicates are positive.
This is not a green-site result and does not prove universal leak protection.

The current 8-bit pixel transform preserves the upper seven bits and chooses
the low bit from a profile/coordinate seed. The PNG palette regression requires
that protection to change some channels while remaining repeatable and
idempotent. Exact original-color equality and this intentional change cannot
both hold for a changed channel. More builds of the same policy cannot resolve
that conflict. Per-profile noise alone is not proof of host-renderer anonymity:
upper-bit differences must also be eliminated or bounded by raster controls.

Separately, stock Chrome 151 and 153 Linux controls returned the negative font
classifier result. Those prior CDP-driven controls are not current manual
desktop acceptance, but prevent attributing the entire result to our font
patches. The prepared reproduction has not been sent externally.

## Choices requiring an explicit identity contract

1. Keep seeded readback protection. Continue treating PixelScan's exact-color
   warning as an acknowledged compatibility limitation, not as a resolved gate.
   This does not explain away the separate font result or waive platform tests.
2. Develop a versioned normalized-renderer policy without arbitrary readback
   perturbation. This is not simply switching `canvas.mode` to native: host font
   isolation, renderer normalization, source snapshots and network protections
   must remain. It changes whether profiles intentionally have distinct canvas
   output; migration and fingerprint regeneration semantics must be defined.

No default switch should occur merely to obtain a green detector screenshot.

## Acceptance before shipping a replacement policy

- Define whether matching profiles share a cohort rendering identity or require
  distinct outputs, and how renderer-policy versions affect existing profiles.
- Compare complete buffers across actual CPU/OS builds, not only CPU feature
  toggles on one host or older macOS references. Retain negative controls that
  demonstrate the harness detects host/raster differences.
- Exercise Window, Worker, OffscreenCanvas, source uploads, readbacks, PNG,
  color formats, alpha, transforms, text and fallback fonts without site cases.
- Preserve origin-clean security and verify actual WebRTC/proxy fail-closed
  behavior on the resulting packaged runtime. Existing 063 network proof does
  not automatically certify a future binary.
- Test actual installed macOS and Windows runtimes as well as Linux. Complete
  desktop authentication/lifecycle and use the accepted archive in nextctl CI.
- Record a fresh uninstrumented site result independently of diagnostic traces.
  A negative external classifier remains negative even if internal tests pass.

These are release gates, not promises that PixelScan will become green. The
external font classifier may require independent clarification or another
reproducible defect. Changing reported font names without real rendering
support, opening the host font catalog, and detector-specific exceptions are
excluded.
