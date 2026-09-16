# Windows font isolation: implementation gap, not just an unrun test

Audit base: PR31 `f7c3cc4`, Chromium 151.0.7922.109 at
`28a7a6c409e03c701d3474ef9e3b1f0be6249039`.

The Linux/macOS closed font catalog must not be described as implemented on
Windows. This is a source-level finding; no current Windows executable has
been tested here and no claim of a reproduced Windows host-font leak is made.

## Evidence

- `clawbrowser/BUILD.gn`: catalog generation and the core catalog dependency
  are guarded by `is_linux || is_mac`. The catalog-backed typeface adapter is
  included only for `is_mac`.
- `clawbrowser/startup.cc`: catalog capability and cached-profile font-list
  reconciliation are guarded by `IS_LINUX || IS_MAC`; Windows resets the
  catalog capability.
- Patch 035 guards explicit local-name probing and fallback-list retries.
  It is not a replacement for the platform character-fallback implementation.
- Patch 039 replaces macOS family lookup, character fallback and last-resort
  fallback. It does not modify the Windows implementations.
- The pinned Windows `font_cache_skia_win.cc` still uses
  `GetFallbackFamilyNameFromHardcodedChoices()` and
  `GetDWriteFallbackFamily()`, which calls the default system font manager's
  `matchFamilyStyleCharacter()`. `CreateFontPlatformData()` similarly retains
  native lookup. Shared Skia last-resort fallback is a separate path to audit.
- `scripts/build_windows_prod_clawbrowser.ps1` does not stage
  `clawbrowser-fonts`. The setup archive packages `mini_installer.exe`, so
  adding a folder to the portable ZIP alone would not fix installed builds.
- Catalog provenance/dynamic-font tests currently skip Windows explicitly.
  The Windows raster portability CI job tests numerical code, not this path.

## Required implementation and acceptance

1. Extract the platform-neutral catalog family/character matching from the
   macOS adapter without regressing its alias/style and collection behavior.
2. Add a validated Windows catalog resource path with renderer sandbox-safe
   access. Missing/corrupt assets must fail closed for managed profiles.
   Do not globally register fonts or disable the sandbox to make tests pass.
3. Connect Windows family lookup, character fallback and last-resort fallback
   to that catalog in managed mode. Preserve downloaded web fonts and native
   behavior when protection is intentionally not enabled.
4. Generate the same pinned assets for Windows and include them in both the
   portable package and actual mini-installer payload. Validate installed
   paths and manifest/font hashes, not just build-output files.
5. Enable applicable Windows integration tests only after the implementation
   exists. Add native-positive controls, two genuinely different host font
   inventories, explicit-local-name probes, multilingual fallback, DOM/Worker
   add/delete/clear and fresh/retained canvas tests.
6. Run the installed Windows ClawBrowser with its sandbox enabled and compare
   protected observations to Linux/macOS. Skips and source checks cannot
   satisfy this acceptance gate.

No Windows privacy-completion or all-platform merge recommendation is justified
until this gap is implemented and the actual binary acceptance passes.
