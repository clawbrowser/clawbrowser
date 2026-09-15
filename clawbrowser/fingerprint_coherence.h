#ifndef CLAWBROWSER_FINGERPRINT_COHERENCE_H_
#define CLAWBROWSER_FINGERPRINT_COHERENCE_H_

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>

#include "clawbrowser/fingerprint_accessor.h"
#include "build/build_config.h"
#include "clawbrowser/font_catalog_identity.h"
#include "clawbrowser/noise/prng.h"

// Helpers shared between the Chromium patches and the standalone shim so the
// surfaces they spoof cannot contradict each other. Everything here is inline
// and header-only on purpose: Blink includes this from
// //third_party/blink/renderer/{core,modules,platform}, which depend only on
// :clawbrowser_runtime, so pulling in a separate translation unit would add a
// link dependency those targets do not carry.
//
// Keeping the logic here (rather than inside the .patch files) also means it is
// reachable from clawbrowser_unittests -- patch bodies are not.

namespace clawbrowser {

// ---------------------------------------------------------------------------
// Local font policy
// ---------------------------------------------------------------------------

// True when the fingerprint policy requires local-font filtering. An empty
// allowlist means "allow no concrete local fonts"; treating it as disabled
// would turn a malformed or partially migrated profile into a host-font leak.
inline bool ShouldFilterLocalFonts(const RuntimeFingerprint& fp) {
  return fp.surface_policy.fonts == "native_or_allowlist" ||
         fp.surface_policy.fonts == "override";
}

// Case-insensitive comparison; CSS family names and the Font Access table
// disagree on casing for the same physical font.
inline bool FontNameMatches(std::string_view a, std::string_view b) {
  return a.size() == b.size() &&
         std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
           const auto lower = [](char c) {
             return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
           };
           return lower(x) == lower(y);
         });
}

// True when `name` (a PostScript name, full name, or CSS family) is present in
// the fingerprint's allowlist. Callers should gate on ShouldFilterLocalFonts().
inline bool IsLocalFontAllowed(const RuntimeFingerprint& fp,
                               std::string_view name) {
  if (fp.fonts.empty()) {
    return false;
  }
  for (const auto& allowed : fp.fonts) {
    if (FontNameMatches(allowed, name)) {
      return true;
    }
  }
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  const auto family = LinuxFontAliasFamily(name);
  for (const auto& allowed : fp.fonts) {
    if (!family.empty() && FontNameMatches(allowed, family)) return true;
  }
#endif
  return false;
}

// One-call predicate for the font-matching hooks: true when a concrete local
// family or unique-name lookup must be refused for the loaded fingerprint.
// Call sites that hold a FontFamily must exempt generic families using Blink's
// FamilyIsGeneric bit. A string such as `local("serif")` is a literal unique
// name, not generic CSS syntax, and therefore must not be exempted here.
//
// Blink resolves a family through CSSFontSelector and, if that returns null,
// retries against FontCache directly (font_fallback_list.cc). Filtering only
// the selector therefore closes nothing -- every refusal is undone by the
// retry. Both sites must consult this.
inline bool IsLocalFontBlocked(std::string_view name) {
  const RuntimeFingerprint* fp = FingerprintAccessor::Get();
  if (!fp || !ShouldFilterLocalFonts(*fp)) {
    return false;
  }
  return !IsLocalFontAllowed(*fp, name);
}

// ---------------------------------------------------------------------------
// Battery coherence
// ---------------------------------------------------------------------------

// BatteryManager exposes four values that the spec ties together. Spoofing only
// `charging` and `level` while chargingTime/dischargingTime keep reporting the
// host's real battery produces impossible combinations -- a machine that claims
// to be charging while still reporting a finite time until empty, for instance.
//
// Invariants enforced here (see
// https://w3c.github.io/battery/#dom-batterymanager-chargingtime):
//   * charging  -> dischargingTime is +Infinity
//   * !charging -> chargingTime is +Infinity
//   * charging && level == 1.0 -> chargingTime is 0
struct BatteryTimes {
  double charging_time = 0.0;
  double discharging_time = 0.0;
};

inline double BatteryInfinity() {
  return std::numeric_limits<double>::infinity();
}

// Derives a plausible, internally consistent pair of times from the spoofed
// charging state and level. `seed` keeps the result stable for a given
// fingerprint instead of varying per call, which would itself be observable.
inline BatteryTimes DeriveBatteryTimes(bool charging, double level,
                                       uint64_t seed) {
  BatteryTimes times;
  const double clamped = std::clamp(level, 0.0, 1.0);
  Prng prng(seed ? seed : 1);

  if (charging) {
    times.discharging_time = BatteryInfinity();
    if (clamped >= 1.0) {
      times.charging_time = 0.0;
      return times;
    }
    // Full charge in roughly 30-150 minutes, scaled by how empty it is, then
    // rounded to whole minutes -- real implementations report coarse values.
    const double span = 1800.0 + (prng.NextUint32() % 7201);
    times.charging_time = std::floor((1.0 - clamped) * span / 60.0) * 60.0;
    return times;
  }

  times.charging_time = BatteryInfinity();
  // Between roughly 1 and 9 hours at full charge, scaled by remaining level.
  const double span = 3600.0 + (prng.NextUint32() % 28801);
  times.discharging_time = std::floor(clamped * span / 60.0) * 60.0;
  return times;
}

// Resolves the effective times for a fingerprint, preferring explicit backend
// values and falling back to the derived pair. Values supplied by the backend
// are still forced to satisfy the invariants above, so a partially-populated
// or inconsistent payload cannot reintroduce a contradiction.
inline BatteryTimes ResolveBatteryTimes(const RuntimeBattery& battery,
                                        uint64_t seed) {
  const bool charging = battery.charging.value_or(true);
  const double level = battery.level.value_or(1.0);
  BatteryTimes times = DeriveBatteryTimes(charging, level, seed);

  if (battery.charging_time.has_value() && !charging) {
    // Ignored: a discharging battery must report Infinity.
  } else if (battery.charging_time.has_value()) {
    times.charging_time = *battery.charging_time;
  }

  if (battery.discharging_time.has_value() && charging) {
    // Ignored: a charging battery must report Infinity.
  } else if (battery.discharging_time.has_value()) {
    times.discharging_time = *battery.discharging_time;
  }

  return times;
}

// ---------------------------------------------------------------------------
// Window / screen coherence
// ---------------------------------------------------------------------------

// A spoofed screen is only believable if the real window still fits inside it.
// Detectors compare window.outerWidth/outerHeight against screen.availWidth/
// availHeight; a window larger than the screen it claims to be on is an
// immediate contradiction. Returns the outer window size to request, clamped
// into the spoofed available area and left slightly inset so the window does
// not exactly fill the screen (which is itself unusual for a restored window).
struct WindowSize {
  int width = 0;
  int height = 0;
};

inline WindowSize DeriveWindowSize(int avail_width, int avail_height,
                                   uint64_t seed) {
  WindowSize size;
  if (avail_width <= 0 || avail_height <= 0) {
    return size;
  }

  Prng prng(seed ? seed : 1);
  // Occupy most of the available area, varying per fingerprint so every
  // profile does not report an identical window.
  const int width_inset = static_cast<int>(prng.NextUint32() % 121);   // 0-120
  const int height_inset = static_cast<int>(prng.NextUint32() % 161);  // 0-160

  size.width = std::max(400, avail_width - width_inset);
  size.height = std::max(300, avail_height - height_inset);
  size.width = std::min(size.width, avail_width);
  size.height = std::min(size.height, avail_height);
  return size;
}

}  // namespace clawbrowser

#endif  // CLAWBROWSER_FINGERPRINT_COHERENCE_H_
