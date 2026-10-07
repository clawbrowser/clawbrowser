#ifndef CLAWBROWSER_FONT_CATALOG_IDENTITY_H_
#define CLAWBROWSER_FONT_CATALOG_IDENTITY_H_

#include <string>
#include <string_view>
#include <vector>
#include "base/strings/string_util.h"

namespace clawbrowser {
// Describes the Linux bundle, not the host OS's installed font inventory.
inline constexpr char kLinuxFontCatalogID[] = "clawbrowser-fonts-prototype-3";
inline std::vector<std::string> LinuxFontCatalogFamilies() {
  return {"Arimo", "Tinos", "Cousine", "DejaVu Sans",
          "Noto Sans CJK JP", "Noto Sans CJK KR", "Noto Sans CJK SC",
          "Noto Sans CJK TC", "Noto Sans CJK HK",
          "Lohit Devanagari", "Noto Sans Thai",
          "Noto Color Emoji", "Noto Sans Bengali", "Noto Sans Ethiopic", "Noto Sans Gujarati", "Noto Sans Gurmukhi", "Noto Sans Kannada", "Noto Sans Khmer", "Noto Sans Malayalam", "Noto Sans Myanmar", "Noto Sans Sinhala", "Noto Sans Tamil", "Noto Sans Telugu"};
}
// Full/PostScript names are distinct from CSS family names. Match exact
// shipped aliases, never arbitrary suffixes supplied by a page.
inline std::string LinuxFontAliasFamily(std::string_view name) {
  for (const auto& script : {"Bengali", "Ethiopic", "Gujarati", "Gurmukhi",
                             "Kannada", "Khmer", "Malayalam", "Myanmar",
                             "Sinhala", "Tamil", "Telugu"}) {
    const std::string family = std::string("Noto Sans ") + script;
    if (base::EqualsCaseInsensitiveASCII(name, family + " Regular") ||
        base::EqualsCaseInsensitiveASCII(name,
            std::string("NotoSans") + script + "-Regular")) return family;
  }
  if (base::EqualsCaseInsensitiveASCII(name, "NotoSansThai-Regular") ||
      base::EqualsCaseInsensitiveASCII(name, "Noto Sans Thai Regular")) {
    return "Noto Sans Thai";
  }
  if (base::EqualsCaseInsensitiveASCII(name, "Lohit-Devanagari")) {
    return "Lohit Devanagari";
  }
  if (base::EqualsCaseInsensitiveASCII(name, "DejaVuSans")) {
    return "DejaVu Sans";
  }
  for (const auto& family : {"Arimo", "Tinos", "Cousine"}) {
    for (const auto& style : {"Regular", "Bold", "Italic", "BoldItalic"}) {
      if (base::EqualsCaseInsensitiveASCII(name, std::string(family) + "-" + style) ||
          base::EqualsCaseInsensitiveASCII(name, std::string(family) + " " + style) ||
          (std::string_view(style) == "BoldItalic" &&
           base::EqualsCaseInsensitiveASCII(name, std::string(family) + " Bold Italic"))) {
        return family;
      }
    }
  }
  return {};
}
inline bool IsBundledLinuxFontName(std::string_view name) {
  for (const auto& family : LinuxFontCatalogFamilies()) {
    if (base::EqualsCaseInsensitiveASCII(name, family)) return true;
  }
  return !LinuxFontAliasFamily(name).empty();
}

// A Windows fingerprint on a Windows host renders with the host's own fonts,
// limited to families that every Windows 10 and Windows 11 install ships
// (learn.microsoft.com/typography/fonts/windows_10_font_list, desktop set).
// Optional language packs, Office, Win11-only families (Segoe UI Variable,
// Segoe Fluent Icons) and anything the user installed stay invisible.
inline constexpr char kWindowsHostFontCatalogID[] = "windows-host-default-1";
inline std::vector<std::string> WindowsDefaultFontFamilies() {
  return {"Arial", "Arial Black", "Bahnschrift", "Calibri", "Calibri Light",
          "Cambria", "Cambria Math", "Candara", "Candara Light",
          "Comic Sans MS", "Consolas", "Constantia", "Corbel", "Corbel Light",
          "Courier New", "Ebrima", "Franklin Gothic Medium", "Gabriola",
          "Gadugi", "Georgia", "Impact", "Ink Free", "Javanese Text",
          "Leelawadee UI", "Leelawadee UI Semilight", "Lucida Console",
          "Lucida Sans Unicode", "Malgun Gothic", "Malgun Gothic Semilight",
          "Marlett", "Microsoft Himalaya", "Microsoft JhengHei",
          "Microsoft JhengHei Light", "Microsoft JhengHei UI",
          "Microsoft JhengHei UI Light", "Microsoft New Tai Lue",
          "Microsoft PhagsPa", "Microsoft Sans Serif", "Microsoft Tai Le",
          "Microsoft YaHei", "Microsoft YaHei Light", "Microsoft YaHei UI",
          "Microsoft YaHei UI Light", "Microsoft Yi Baiti", "MingLiU-ExtB",
          "MingLiU_HKSCS-ExtB", "Mongolian Baiti", "MS Gothic", "MS PGothic",
          "MS UI Gothic", "MV Boli", "Myanmar Text", "Nirmala UI",
          "Nirmala UI Semilight", "NSimSun", "Palatino Linotype",
          "PMingLiU-ExtB", "Segoe MDL2 Assets", "Segoe Print", "Segoe Script",
          "Segoe UI", "Segoe UI Black", "Segoe UI Emoji", "Segoe UI Historic",
          "Segoe UI Light", "Segoe UI Semibold", "Segoe UI Semilight",
          "Segoe UI Symbol", "SimSun", "SimSun-ExtB", "Sitka Banner",
          "Sitka Display", "Sitka Heading", "Sitka Small", "Sitka Subheading",
          "Sitka Text", "Sylfaen", "Symbol", "Tahoma", "Times New Roman",
          "Trebuchet MS", "Verdana", "Webdings", "Wingdings", "Yu Gothic",
          "Yu Gothic Light", "Yu Gothic Medium", "Yu Gothic UI",
          "Yu Gothic UI Light", "Yu Gothic UI Semibold",
          "Yu Gothic UI Semilight"};
}
inline bool IsWindowsDefaultFontName(std::string_view name) {
  for (const auto& family : WindowsDefaultFontFamilies()) {
    if (base::EqualsCaseInsensitiveASCII(name, family)) return true;
  }
  return false;
}
// Accept either field: older backends filled only navigator.platform.
inline bool IsWindowsFingerprint(std::string_view os,
                                 std::string_view platform) {
  return base::EqualsCaseInsensitiveASCII(os, "windows") ||
         base::EqualsCaseInsensitiveASCII(platform, "Win32");
}

struct RuntimeFontSelection {
  std::vector<std::string> fonts;
  std::string catalog_id;
};
// The installed runtime is authoritative even with cached profiles or an
// older backend that ignores the capability hint.
//
// A Windows fingerprint on a Windows host always gets the full default list:
// a stock Windows install exposes all of it, and the backend's subset would
// drop the emoji, CJK, Indic and Thai fallback families, which host-font
// filtering would then render as tofu. Elsewhere, keep supported explicit
// subsets of the bundle; replace an entirely incompatible legacy list with it.
inline RuntimeFontSelection SelectRuntimeFonts(
    const std::vector<std::string>& profile_fonts,
    std::string_view fingerprint_os,
    std::string_view fingerprint_platform,
    bool windows_host) {
  RuntimeFontSelection selection;
  if (windows_host &&
      IsWindowsFingerprint(fingerprint_os, fingerprint_platform)) {
    selection.catalog_id = kWindowsHostFontCatalogID;
    selection.fonts = WindowsDefaultFontFamilies();
    return selection;
  }
  selection.catalog_id = kLinuxFontCatalogID;
  for (const auto& name : profile_fonts) {
    if (IsBundledLinuxFontName(name)) selection.fonts.push_back(name);
  }
  if (selection.fonts.empty()) selection.fonts = LinuxFontCatalogFamilies();
  return selection;
}
}  // namespace clawbrowser
#endif
