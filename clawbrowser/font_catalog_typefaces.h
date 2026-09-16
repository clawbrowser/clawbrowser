#ifndef CLAWBROWSER_FONT_CATALOG_TYPEFACES_H_
#define CLAWBROWSER_FONT_CATALOG_TYPEFACES_H_

#include <string>
#include <string_view>
#include <vector>
#include "base/types/expected.h"
#include "clawbrowser/font_catalog.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace clawbrowser {
struct CatalogTypeface {
  std::string family;
  SkFontStyle style;
  int collection_index;
  int instance_index;
  sk_sp<SkTypeface> typeface;
};

// Instantiates only checked catalog bytes through Fontations. Enumerates raw
// collection faces and their named variations; never consults a host font mgr.
// Missing/invalid fonts fail the entire catalog rather than silently omitting
// a family and permitting an accidental system fallback.
base::expected<std::vector<CatalogTypeface>, std::string>
CreateCatalogTypefaces(const ValidatedFontCatalog& catalog);

sk_sp<SkTypeface> MatchCatalogFamily(
    const std::vector<CatalogTypeface>& faces,
    std::string_view family,
    const SkFontStyle& style);

// Tries only the supplied families in order, selecting the closest CSS style
// among faces that contain the character. No match returns nullptr; callers
// must not reinterpret that as permission to consult the host font manager.
sk_sp<SkTypeface> MatchCatalogCharacter(
    const std::vector<CatalogTypeface>& faces,
    const std::vector<std::string>& families,
    const SkFontStyle& style,
    SkUnichar character);

// Shared managed-catalog policy for platform adapters. Resolves only exact
// shipped aliases and generic mappings, never the platform font manager.
sk_sp<SkTypeface> MatchManagedCatalogFamily(
    const std::vector<CatalogTypeface>& faces,
    std::string_view family,
    const SkFontStyle& style);
sk_sp<SkTypeface> MatchManagedCatalogCharacter(
    const std::vector<CatalogTypeface>& faces,
    SkUnichar character,
    const SkFontStyle& style,
    bool emoji);
}  // namespace clawbrowser
#endif
