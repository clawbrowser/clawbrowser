#include "clawbrowser/mac_font_catalog.h"
#include "base/apple/bundle_locations.h"
#include "base/check.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "clawbrowser/font_catalog_identity.h"
#include "font_catalog_build.h"

namespace clawbrowser {
namespace {
const std::vector<CatalogTypeface>& Faces() {
  // The sandbox already allows reads inside the signed app bundle. No host
  // font registration or extra filesystem entitlement is needed. Keep only
  // the SkData-backed faces after initialization, not the duplicate snapshot.
  static const base::NoDestructor<std::vector<CatalogTypeface>> faces([] {
    auto snapshot = LoadValidatedFontCatalog(
        base::apple::FrameworkBundlePath().AppendASCII("Resources").AppendASCII(kFontCatalogDirectory),
        kFontCatalogManifestHash);
    CHECK(snapshot.has_value()) << "Managed font catalog validation failed";
    auto loaded = CreateCatalogTypefaces(*snapshot);
    CHECK(loaded.has_value()) << "Managed font catalog decoding failed";
    return std::move(*loaded);
  }());
  return *faces;
}
}  // namespace

sk_sp<SkTypeface> MacCatalogFamily(std::string_view family, const SkFontStyle& style) {
  const auto alias = LinuxFontAliasFamily(family);
  if (!alias.empty()) {
    const auto lower = base::ToLowerASCII(family);
    const bool bold = lower.find("bold") != std::string::npos;
    const bool italic = lower.find("italic") != std::string::npos;
    return MatchCatalogFamily(
        Faces(), alias,
        SkFontStyle(bold ? 700 : 400, SkFontStyle::kNormal_Width,
                    italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant));
  }
  auto name = base::ToLowerASCII(family);
  if (name == "serif" || name == "times" || name == "times new roman")
    return MatchCatalogFamily(Faces(), "Tinos", style);
  if (name == "monospace" || name == "courier" || name == "courier new")
    return MatchCatalogFamily(Faces(), "Cousine", style);
  if (name == "sans" || name == "sans-serif" || name == "arial" ||
      name == "helvetica" || name == "system-ui" || name == "-apple-system" ||
      name == "blinkmacsystemfont" || name == ".applesystemuifont")
    return MatchCatalogFamily(Faces(), "Arimo", style);
  return MatchCatalogFamily(Faces(), family, style);
}

sk_sp<SkTypeface> MacCatalogCharacter(SkUnichar character, const SkFontStyle& style, bool emoji) {
  auto families = LinuxFontCatalogFamilies();
  if (emoji) families.insert(families.begin(), "Noto Color Emoji");
  return MatchCatalogCharacter(Faces(), families, style, character);
}
}  // namespace clawbrowser
