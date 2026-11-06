# Reproducible Windows MIDL library-name index

On a Russian Windows host, Microsoft's MIDL generated a typelib whose library
name used the host's Russian name hash while the typelib declared English LCID
0x409. The other names used the declared locale. Chromium's strict comparison
against checked-in outputs correctly stopped the build.

Patch 065 canonicalizes only this derived library-name hash and its index using
the typelib's existing LCID and Windows' own `LHashValOfNameSysA`. It first
requires the differing hash to match the current user's locale and validates
the complete existing name-index ordering. Unexpected data remains an error.

The IDL, COM GUIDs, methods, declared locale, generated headers and checked-in
binary baselines remain unchanged. Chromium's full output comparison remains
enabled. No system or user locale preference is changed.

Validation on the affected host:

- Original MIDL action failed with a ten-byte name-index difference.
- An SDK-only change and explicit-locale probes did not solve the mismatch;
  experimental IDL/header changes were restored.
- The patched action passed the unchanged full-output comparison.
- `python scripts/test_windows_midl_locale.py` checks actual captured output
  against the entire pinned binary and proves GUID mutations, unexplained
  hashes and index corruption are not hidden.

This is a compiler reproducibility fix, not native browser/runtime acceptance.
