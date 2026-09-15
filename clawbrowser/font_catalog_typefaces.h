#ifndef CLAWBROWSER_FONT_CATALOG_TYPEFACES_H_
#define CLAWBROWSER_FONT_CATALOG_TYPEFACES_H_

#include <string>
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
}  // namespace clawbrowser
#endif
