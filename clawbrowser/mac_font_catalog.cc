#include "clawbrowser/mac_font_catalog.h"
#include "base/apple/bundle_locations.h"
#include "base/check.h"
#include "base/no_destructor.h"
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
  return MatchManagedCatalogFamily(Faces(), family, style);
}

sk_sp<SkTypeface> MacCatalogCharacter(SkUnichar character, const SkFontStyle& style, bool emoji) {
  return MatchManagedCatalogCharacter(Faces(), character, style, emoji);
}
}  // namespace clawbrowser
