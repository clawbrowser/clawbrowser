#ifndef CLAWBROWSER_FONT_CATALOG_IDENTITY_H_
#define CLAWBROWSER_FONT_CATALOG_IDENTITY_H_

#include <string>
#include <string_view>
#include <vector>
#include "base/strings/string_util.h"

namespace clawbrowser {
// Describes the Linux bundle, not the host OS's installed font inventory.
inline constexpr char kLinuxFontCatalogID[] = "clawbrowser-fonts-prototype-2";
inline std::vector<std::string> LinuxFontCatalogFamilies() {
  return {"Arimo", "Tinos", "Cousine", "DejaVu Sans",
          "Noto Sans CJK JP", "Noto Sans CJK KR", "Noto Sans CJK SC",
          "Noto Sans CJK TC", "Noto Sans CJK HK",
          "Lohit Devanagari", "Noto Sans Thai"};
}
// Full/PostScript names are distinct from CSS family names. Match exact
// shipped aliases, never arbitrary suffixes supplied by a page.
inline std::string LinuxFontAliasFamily(std::string_view name) {
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
}  // namespace clawbrowser
#endif
