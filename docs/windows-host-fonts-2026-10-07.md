# Windows host fonts for Windows profiles

Decision date: 2026-10-07. This is a narrow exception to the "opening the host
font catalog" exclusion in `pixelscan-policy-decision-2026-09-16.md`.

## Problem

On Windows, every protected profile rendered from the bundled Linux catalog
(Arimo, Tinos, Noto...). A Windows user agent combined with Linux glyphs and
no Segoe UI or Calibri reads as Linux to font classifiers.

## Policy

A Windows fingerprint on a Windows host renders with the host's own
DirectWrite fonts, limited to `WindowsDefaultFontFamilies()`. That list holds
only families that every Windows 10 and Windows 11 install ships. Language
packs and Features on Demand (Meiryo, Gulim...), Office fonts, Win11-only
families (Segoe UI Variable, Segoe Fluent Icons) and user-installed fonts stay
invisible.

All other combinations keep the closed catalog: Linux and macOS hosts, a
non-Windows fingerprint on Windows, and any profile whose allowlist contains a
name outside the default list.

## Implementation

- `SelectRuntimeFonts()` (startup) sets the profile's font list to the full
  default list and records `runtime_font_catalog = "windows-host-default-1"`.
  It does not keep the backend's Windows preset: those are about 15 families
  without the emoji, CJK, Indic and Thai fallback families (Segoe UI Emoji,
  Microsoft YaHei, Yu Gothic, Nirmala UI...), so the allowlist would refuse
  them and fallback text would render as tofu. A stock Windows install
  exposes the full list anyway.
- `UsesWindowsHostFonts()` / `UsesManagedFontCatalog()` derive the mode from
  the loaded fingerprint, so child processes need no extra switch.
- Patch 062 makes the catalog hooks from 043, 047, 053, 054 and 055 step aside
  in host mode, so a real Windows profile does not get Linux strike, metrics or
  system-font behavior.
- Host mode keeps the name filters from 011, 034, 035 and 056.
  `CreateFontPlatformData` refuses unlisted families. Hardcoded and DirectWrite
  character fallback both reach that function, so fallback cannot pick a
  non-default font either. `system-ui` is pinned to Segoe UI.

## Not verified

None of this has been compiled or run on Windows yet. Acceptance needs:

- an installed Windows build with the sandbox enabled;
- one host with extra fonts and one without them, giving identical results;
- multilingual and emoji fallback;
- `local()` probes;
- a fresh pixelscan run.
