#include "clawbrowser/fingerprint_coherence.h"

#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

TEST(BundledFontIdentityTest, RecognizesOnlyShippedNames) {
  EXPECT_TRUE(IsBundledLinuxFontName("arimo"));
  EXPECT_TRUE(IsBundledLinuxFontName("Tinos-Bold"));
  EXPECT_TRUE(IsBundledLinuxFontName("Noto Sans Thai Regular"));
  EXPECT_TRUE(IsBundledLinuxFontName("NotoSansBengali-Regular"));
  EXPECT_TRUE(IsBundledLinuxFontName("Noto Sans Khmer Regular"));
  EXPECT_TRUE(IsBundledLinuxFontName("Noto Color Emoji"));
  EXPECT_FALSE(IsBundledLinuxFontName("NotoSansBengali-Bold"));
  EXPECT_TRUE(IsBundledLinuxFontName("NotoSansThai-Regular"));
  EXPECT_EQ(LinuxFontAliasFamily("DejaVuSans"), "DejaVu Sans");
  EXPECT_EQ(LinuxFontAliasFamily("Lohit-Devanagari"), "Lohit Devanagari");
  EXPECT_FALSE(IsBundledLinuxFontName("Bitstream Vera Sans Mono"));
  EXPECT_FALSE(IsBundledLinuxFontName("Arimo-ArbitrarySuffix"));
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
TEST(BundledFontIdentityTest, FamilyAllowsShippedUniqueNamesOnly) {
  RuntimeFingerprint fp;
  fp.fonts = {"Arimo"};
  EXPECT_TRUE(IsLocalFontAllowed(fp, "arimo-regular"));
  EXPECT_TRUE(IsLocalFontAllowed(fp, "Arimo Bold"));
  EXPECT_FALSE(IsLocalFontAllowed(fp, "Tinos-Regular"));
  EXPECT_FALSE(IsLocalFontAllowed(fp, "Arimo-Fake"));
}
#endif

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

TEST(LocalFontPolicyTest, ProtectedEmptyAllowlistStillFilters) {
  EXPECT_TRUE(ShouldFilterLocalFonts(MakeFingerprint({}, "override")));
  EXPECT_TRUE(ShouldFilterLocalFonts(MakeFingerprint({}, "native_or_allowlist")));
  EXPECT_FALSE(ShouldFilterLocalFonts(MakeFingerprint({}, "native")));
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

TEST(LocalFontPolicyTest, EmptyProtectedAllowlistAllowsNothing) {
  const RuntimeFingerprint fp = MakeFingerprint({}, "override");
  EXPECT_FALSE(IsLocalFontAllowed(fp, "Anything"));
}

TEST(LocalFontPolicyTest, LiteralGenericNameDoesNotBypassLocalSourceFilter) {
  FingerprintAccessor::Set(MakeFingerprint({"Arial"}, "override"),
                           std::nullopt);
  EXPECT_TRUE(IsLocalFontBlocked("serif"));
  EXPECT_TRUE(IsLocalFontBlocked(""));
  EXPECT_FALSE(IsLocalFontBlocked("Arial"));
  FingerprintAccessor::Reset();
}

RuntimeFingerprint MakeWindowsFingerprint(std::vector<std::string> fonts) {
  RuntimeFingerprint fp =
      MakeFingerprint(std::move(fonts), "native_or_allowlist");
  fp.os = "windows";
  fp.platform = "Win32";
  return fp;
}

TEST(WindowsHostFontsTest, DefaultListCoversCoreFamiliesOnly) {
  EXPECT_TRUE(IsWindowsDefaultFontName("Segoe UI"));
  EXPECT_TRUE(IsWindowsDefaultFontName("calibri"));
  // Language-pack, Office, Win11-only and catalog names stay out.
  EXPECT_FALSE(IsWindowsDefaultFontName("Meiryo"));
  EXPECT_FALSE(IsWindowsDefaultFontName("Aptos"));
  EXPECT_FALSE(IsWindowsDefaultFontName("Segoe UI Variable"));
  EXPECT_FALSE(IsWindowsDefaultFontName("Arimo"));
  EXPECT_FALSE(IsWindowsDefaultFontName("Segoe"));
}

TEST(WindowsHostFontsTest, FullWindowsListCarriesFallbackFamilies) {
  for (const char* family :
       {"Segoe UI Emoji", "Segoe UI Symbol", "Microsoft YaHei", "Yu Gothic",
        "Malgun Gothic", "Nirmala UI", "Leelawadee UI", "Ebrima"}) {
    EXPECT_TRUE(IsWindowsDefaultFontName(family)) << family;
  }
}

TEST(WindowsHostFontsTest, SelectionUsesFullWindowsListOnWindowsHost) {
  RuntimeFontSelection selection = SelectRuntimeFonts(
      {"Calibri", "Fira Code", "Segoe UI", "Arimo"}, "windows", "Win32",
      /*windows_host=*/true);
  EXPECT_EQ(selection.catalog_id, kWindowsHostFontCatalogID);
  EXPECT_EQ(selection.fonts, WindowsDefaultFontFamilies());

  selection =
      SelectRuntimeFonts({"Arimo", "Tinos"}, "", "Win32", /*windows_host=*/true);
  EXPECT_EQ(selection.catalog_id, kWindowsHostFontCatalogID);
  EXPECT_EQ(selection.fonts, WindowsDefaultFontFamilies());
}

TEST(WindowsHostFontsTest, SelectionKeepsOnlyInstalledDefaultFamilies) {
  // Windows Server lacks many desktop fonts (Candara, Sitka, Yu Gothic...).
  const std::vector<std::string> installed = {"segoe ui", "Calibri", "Arial",
                                              "Fira Code", "Segoe UI Emoji"};
  RuntimeFontSelection selection =
      SelectRuntimeFonts({"Calibri"}, "windows", "Win32",
                         /*windows_host=*/true, &installed);
  EXPECT_EQ(selection.catalog_id, kWindowsHostFontCatalogID);
  // Default-list order and spelling; never the extra host font.
  EXPECT_EQ(selection.fonts, (std::vector<std::string>{
                                 "Arial", "Calibri", "Segoe UI",
                                 "Segoe UI Emoji"}));
}

TEST(WindowsHostFontsTest, SelectionWithoutInstalledDefaultsUsesCatalog) {
  const std::vector<std::string> installed = {"Fira Code"};
  RuntimeFontSelection selection =
      SelectRuntimeFonts({"Calibri", "Arimo"}, "windows", "Win32",
                         /*windows_host=*/true, &installed);
  EXPECT_EQ(selection.catalog_id, kLinuxFontCatalogID);
  EXPECT_EQ(selection.fonts, (std::vector<std::string>{"Arimo"}));
}

TEST(WindowsHostFontsTest, SelectionIgnoresInstalledListOffWindowsHost) {
  const std::vector<std::string> installed = {"Arial"};
  RuntimeFontSelection selection =
      SelectRuntimeFonts({"Arimo"}, "windows", "Win32",
                         /*windows_host=*/false, &installed);
  EXPECT_EQ(selection.catalog_id, kLinuxFontCatalogID);
}

TEST(WindowsHostFontsTest, SelectionUsesCatalogOffWindowsOrForOtherOS) {
  RuntimeFontSelection linux_host = SelectRuntimeFonts(
      {"Calibri", "Arimo"}, "windows", "Win32", /*windows_host=*/false);
  EXPECT_EQ(linux_host.catalog_id, kLinuxFontCatalogID);
  EXPECT_EQ(linux_host.fonts, (std::vector<std::string>{"Arimo"}));

  RuntimeFontSelection mac_profile = SelectRuntimeFonts(
      {"Helvetica Neue"}, "macos", "MacIntel", /*windows_host=*/true);
  EXPECT_EQ(mac_profile.catalog_id, kLinuxFontCatalogID);
  EXPECT_EQ(mac_profile.fonts, LinuxFontCatalogFamilies());
}

TEST(WindowsHostFontsTest, HostModeRequiresWindowsHostProfileAndDefaults) {
  const RuntimeFingerprint fp = MakeWindowsFingerprint({"Arial", "Segoe UI"});
  EXPECT_TRUE(UsesWindowsHostFontsOnHost(fp, /*windows_host=*/true));
  EXPECT_FALSE(UsesManagedFontCatalogOnHost(fp, /*windows_host=*/true));

  EXPECT_FALSE(UsesWindowsHostFontsOnHost(fp, /*windows_host=*/false));
  EXPECT_TRUE(UsesManagedFontCatalogOnHost(fp, /*windows_host=*/false));

  RuntimeFingerprint mac = fp;
  mac.os = "macos";
  mac.platform = "MacIntel";
  EXPECT_FALSE(UsesWindowsHostFontsOnHost(mac, /*windows_host=*/true));
}

TEST(WindowsHostFontsTest, AnyNonDefaultNameKeepsTheCatalog) {
  // A stale or hand-edited list must not widen the host's exposed fonts.
  EXPECT_FALSE(UsesWindowsHostFontsOnHost(
      MakeWindowsFingerprint({"Segoe UI", "Arimo"}), /*windows_host=*/true));
  EXPECT_FALSE(UsesWindowsHostFontsOnHost(
      MakeWindowsFingerprint({"Segoe UI", "Fira Code"}),
      /*windows_host=*/true));
  EXPECT_FALSE(UsesWindowsHostFontsOnHost(MakeWindowsFingerprint({}),
                                          /*windows_host=*/true));
}

TEST(WindowsHostFontsTest, NativePolicyIsNeitherHostNorCatalog) {
  RuntimeFingerprint fp = MakeWindowsFingerprint({"Segoe UI"});
  fp.surface_policy.fonts = "native";
  EXPECT_FALSE(UsesWindowsHostFontsOnHost(fp, /*windows_host=*/true));
  EXPECT_FALSE(UsesManagedFontCatalogOnHost(fp, /*windows_host=*/true));
}

TEST(WindowsHostFontsTest, HostModeStillBlocksUnlistedFamilies) {
  const RuntimeFingerprint fp = MakeWindowsFingerprint({"Segoe UI", "Calibri"});
  EXPECT_TRUE(IsLocalFontAllowed(fp, "segoe ui"));
  EXPECT_FALSE(IsLocalFontAllowed(fp, "Meiryo"));
  EXPECT_FALSE(IsLocalFontAllowed(fp, "Arial"));
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
