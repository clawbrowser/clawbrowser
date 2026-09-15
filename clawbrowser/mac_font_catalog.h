#ifndef CLAWBROWSER_MAC_FONT_CATALOG_H_
#define CLAWBROWSER_MAC_FONT_CATALOG_H_
#include "clawbrowser/font_catalog_typefaces.h"
namespace clawbrowser {
sk_sp<SkTypeface> MacCatalogFamily(std::string_view family, const SkFontStyle& style);
sk_sp<SkTypeface> MacCatalogCharacter(SkUnichar character, const SkFontStyle& style, bool emoji);
}  // namespace clawbrowser
#endif
