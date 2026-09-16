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

## Shared-policy preparation

The macOS adapter now delegates family/character selection to platform-neutral
`MatchManagedCatalogFamily` / `MatchManagedCatalogCharacter`. The filesystem
loader remains platform-specific. Alias/style mappings and fallback order were
moved without changes; Windows support is not enabled by this refactor.

The real Chromium `clawbrowser_font_typeface_unittests` target rebuilt on the
Linux QA host (3 build steps, 9.35s). All three tests passed, including checked
catalog assets, generic mappings, exact full/PostScript aliases, named-style
precedence, emoji priority, invalid code points and an empty-catalog negative
control. Report: `shared-font-policy-054.xml`; the pinned-catalog case took
101ms. This does not replace recompiling/testing the macOS adapter or a Windows
binary. The last accepted browser artifact remains worker-font-053.

## Preload and byte-integrity preparation

`LoadCatalogTypefaces` validates and decodes a single owned snapshot. A new
real-font unit test deletes its own temporary catalog after loading and proves
the returned face can still resolve a glyph and expose its owned stream. Wrong
manifest hashes are rejected. The four C++ tests passed on Linux (102ms for
the full pinned catalog), report `preloaded-font-bytes-055.xml`.

A Windows adapter now resolves resources relative to the loaded module, not
the launcher, and has explicit pre-sandbox initialization with no lazy disk
lookup or system-font fallback. GN can generate the pinned Windows catalog.
**This adapter is not yet hooked into renderer startup or Windows FontCache,
and has not been compiled on Windows.** Installer staging is still pending.
The pinned RendererMain loads the fingerprint before `InitializeSkia()`;
Windows sandbox engagement occurs later. Integration must preload after Skia
initialization and before sandbox engagement, only for managed font mode.

The audit also found a platform-specific staging defect: Windows text-mode
newlines could make the stored manifest bytes differ from the hashed canonical
bytes. Manifest/config/marker/header output now uses exact UTF-8 byte writes;
reuse validation compares raw manifest and marker bytes. Two new regressions
fail on the previous implementation (simulated Windows newline translation
and a CRLF-corrupted reused manifest). All 9 build-catalog and 7 staging tests
pass locally. The Windows CI job now runs the build-catalog regressions on a
real Windows runner; this is staging evidence, not browser acceptance.

## Renderer and fallback wiring

Patch 053 connects managed Windows renderer initialization after Skia setup,
before sandbox engagement, and terminates initialization if catalog loading
fails. Family lookup, character fallback and last-resort fallback now have
catalog-only managed branches; native branches remain for unprotected mode.
The catalog hint and cached-profile normalization also include Windows.
The typeface target is a component with exported entry points so renderer
startup and Blink share the same preloaded state in component builds.

`git apply --check` passed on the pinned Chromium tree and the patch was applied
to the QA checkout. Linux rebuilt the affected renderer/Skia/core units and
the four typeface unit tests passed (80ms for the full catalog), report
`windows-component-policy-056.xml`. The browser link/regression is pending;
this does **not** compile or exercise Windows-only branches.

The preceding staging change passed all **9 tests on Windows CI**, run
`35081750192`, job `104747142355` (0.156s). Installer/portable packaging and
actual Windows acceptance remain required before shipping this wiring.
