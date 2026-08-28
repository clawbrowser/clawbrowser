#include "clawbrowser/fingerprint_coherence.h"

#include <cmath>

#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

RuntimeFingerprint MakeFingerprint(std::vector<std::string> fonts,
                                   const std::string& font_policy) {
  RuntimeFingerprint fp;
  fp.fonts = std::move(fonts);
  fp.surface_policy.fonts = font_policy;
  return fp;
}

// ---------------------------------------------------------------------------
// Local font policy
// ---------------------------------------------------------------------------

TEST(LocalFontPolicyTest, NoAllowlistMeansNoFiltering) {
  EXPECT_FALSE(ShouldFilterLocalFonts(MakeFingerprint({}, "override")));
  EXPECT_FALSE(ShouldFilterLocalFonts(MakeFingerprint({}, "native_or_allowlist")));
}

TEST(LocalFontPolicyTest, FilteringRequiresBothAllowlistAndPolicy) {
  EXPECT_TRUE(ShouldFilterLocalFonts(MakeFingerprint({"Arial"}, "override")));
  EXPECT_TRUE(
      ShouldFilterLocalFonts(MakeFingerprint({"Arial"}, "native_or_allowlist")));
  EXPECT_FALSE(ShouldFilterLocalFonts(MakeFingerprint({"Arial"}, "native")));
  EXPECT_FALSE(ShouldFilterLocalFonts(MakeFingerprint({"Arial"}, "")));
}

TEST(LocalFontPolicyTest, AllowlistMatchIsCaseInsensitive) {
  // CSS family names and the Font Access table disagree on casing for the same
  // physical font, so a case-sensitive compare would filter inconsistently
  // between the two surfaces.
  const RuntimeFingerprint fp = MakeFingerprint({"Times New Roman"}, "override");
  EXPECT_TRUE(IsLocalFontAllowed(fp, "Times New Roman"));
  EXPECT_TRUE(IsLocalFontAllowed(fp, "times new roman"));
  EXPECT_TRUE(IsLocalFontAllowed(fp, "TIMES NEW ROMAN"));
}

TEST(LocalFontPolicyTest, NonAllowlistedFontIsRejected) {
  const RuntimeFingerprint fp = MakeFingerprint({"Arial", "Verdana"}, "override");
  EXPECT_FALSE(IsLocalFontAllowed(fp, "Comic Sans MS"));
  EXPECT_FALSE(IsLocalFontAllowed(fp, "Aria"));    // prefix, not a match
  EXPECT_FALSE(IsLocalFontAllowed(fp, "Arial2"));  // suffix, not a match
}

TEST(LocalFontPolicyTest, EmptyAllowlistAllowsEverything) {
  const RuntimeFingerprint fp = MakeFingerprint({}, "override");
  EXPECT_TRUE(IsLocalFontAllowed(fp, "Anything"));
}

TEST(LocalFontPolicyTest, GenericFamiliesAreRecognized) {
  // Generic families must keep resolving or there is no fallback left to
  // render with, which would be far more conspicuous than font probing.
  EXPECT_TRUE(IsGenericFontFamily("serif"));
  EXPECT_TRUE(IsGenericFontFamily("sans-serif"));
  EXPECT_TRUE(IsGenericFontFamily("monospace"));
  EXPECT_TRUE(IsGenericFontFamily("system-ui"));
  EXPECT_TRUE(IsGenericFontFamily("SANS-SERIF"));
  EXPECT_FALSE(IsGenericFontFamily("Arial"));
  EXPECT_FALSE(IsGenericFontFamily(""));
}

// ---------------------------------------------------------------------------
// Battery coherence
// ---------------------------------------------------------------------------

TEST(BatteryCoherenceTest, ChargingImpliesInfiniteDischargingTime) {
  const BatteryTimes times = DeriveBatteryTimes(/*charging=*/true, 0.5, 42);
  EXPECT_TRUE(std::isinf(times.discharging_time));
  EXPECT_FALSE(std::isinf(times.charging_time));
}

TEST(BatteryCoherenceTest, DischargingImpliesInfiniteChargingTime) {
  const BatteryTimes times = DeriveBatteryTimes(/*charging=*/false, 0.5, 42);
  EXPECT_TRUE(std::isinf(times.charging_time));
  EXPECT_FALSE(std::isinf(times.discharging_time));
}

TEST(BatteryCoherenceTest, FullAndChargingReportsZeroChargingTime) {
  // Required by the spec: a full, charging battery reports chargingTime 0.
  const BatteryTimes times = DeriveBatteryTimes(/*charging=*/true, 1.0, 42);
  EXPECT_EQ(0.0, times.charging_time);
  EXPECT_TRUE(std::isinf(times.discharging_time));
}

TEST(BatteryCoherenceTest, DerivedTimesAreStableForSameSeed) {
  // Values that change between reads would be observable on their own.
  const BatteryTimes a = DeriveBatteryTimes(false, 0.42, 1234);
  const BatteryTimes b = DeriveBatteryTimes(false, 0.42, 1234);
  EXPECT_EQ(a.charging_time, b.charging_time);
  EXPECT_EQ(a.discharging_time, b.discharging_time);
}

TEST(BatteryCoherenceTest, DerivedTimesVaryBySeed) {
  EXPECT_NE(DeriveBatteryTimes(false, 0.42, 1).discharging_time,
            DeriveBatteryTimes(false, 0.42, 999).discharging_time);
}

TEST(BatteryCoherenceTest, TimesAreWholeMinutes) {
  // Real implementations report coarse values; a to-the-second reading would
  // stand out.
  for (uint64_t seed = 1; seed < 20; ++seed) {
    const BatteryTimes t = DeriveBatteryTimes(false, 0.5, seed);
    EXPECT_EQ(0.0, std::fmod(t.discharging_time, 60.0));
  }
}

TEST(BatteryCoherenceTest, LevelIsClampedIntoRange) {
  const BatteryTimes high = DeriveBatteryTimes(true, 5.0, 7);
  EXPECT_EQ(0.0, high.charging_time);  // clamped to 1.0 -> treated as full
  const BatteryTimes low = DeriveBatteryTimes(false, -3.0, 7);
  EXPECT_EQ(0.0, low.discharging_time);  // clamped to 0.0 -> empty
}

TEST(BatteryCoherenceTest, ExplicitBackendValuesAreUsedWhenConsistent) {
  RuntimeBattery battery;
  battery.charging = false;
  battery.level = 0.5;
  battery.discharging_time = 4242.0;

  const BatteryTimes times = ResolveBatteryTimes(battery, 1);
  EXPECT_EQ(4242.0, times.discharging_time);
  EXPECT_TRUE(std::isinf(times.charging_time));
}

TEST(BatteryCoherenceTest, ContradictoryBackendValuesAreOverridden) {
  // A charging battery that also claims a finite time-to-empty is exactly the
  // contradiction this is meant to prevent, so the impossible field loses.
  RuntimeBattery battery;
  battery.charging = true;
  battery.level = 0.5;
  battery.discharging_time = 1234.0;

  const BatteryTimes times = ResolveBatteryTimes(battery, 1);
  EXPECT_TRUE(std::isinf(times.discharging_time));
}

TEST(BatteryCoherenceTest, ResolveHandlesAbsentFields) {
  RuntimeBattery battery;  // nothing populated
  const BatteryTimes times = ResolveBatteryTimes(battery, 5);
  // Defaults to charging + full, so both values take their spec-mandated form.
  EXPECT_EQ(0.0, times.charging_time);
  EXPECT_TRUE(std::isinf(times.discharging_time));
}

// ---------------------------------------------------------------------------
// Window / screen coherence
// ---------------------------------------------------------------------------

TEST(WindowCoherenceTest, WindowFitsInsideAvailableArea) {
  // The invariant a detector checks: outerWidth <= screen.availWidth.
  for (uint64_t seed = 1; seed < 40; ++seed) {
    const WindowSize size = DeriveWindowSize(1920, 1040, seed);
    EXPECT_LE(size.width, 1920);
    EXPECT_LE(size.height, 1040);
    EXPECT_GT(size.width, 0);
    EXPECT_GT(size.height, 0);
  }
}

TEST(WindowCoherenceTest, StableForSameSeed) {
  const WindowSize a = DeriveWindowSize(1920, 1040, 77);
  const WindowSize b = DeriveWindowSize(1920, 1040, 77);
  EXPECT_EQ(a.width, b.width);
  EXPECT_EQ(a.height, b.height);
}

TEST(WindowCoherenceTest, VariesAcrossSeeds) {
  // Every profile reporting an identical window would itself be a signal.
  std::set<std::pair<int, int>> sizes;
  for (uint64_t seed = 1; seed < 30; ++seed) {
    const WindowSize s = DeriveWindowSize(1920, 1040, seed);
    sizes.insert({s.width, s.height});
  }
  EXPECT_GT(sizes.size(), 10u);
}

TEST(WindowCoherenceTest, DegenerateScreenYieldsNoOverride) {
  // Nothing sensible to derive; caller should leave the window alone.
  EXPECT_EQ(0, DeriveWindowSize(0, 0, 1).width);
  EXPECT_EQ(0, DeriveWindowSize(-1, 500, 1).width);
}

TEST(WindowCoherenceTest, TinyScreenStillProducesUsableWindow) {
  const WindowSize size = DeriveWindowSize(320, 200, 3);
  EXPECT_GT(size.width, 0);
  EXPECT_GT(size.height, 0);
  EXPECT_LE(size.width, 320);
  EXPECT_LE(size.height, 200);
}

}  // namespace
}  // namespace clawbrowser
