#ifndef CLAWBROWSER_WINDOWS_FONT_CATALOG_H_
#define CLAWBROWSER_WINDOWS_FONT_CATALOG_H_

#include "clawbrowser/font_catalog_typefaces.h"

namespace clawbrowser {
// Must run on the renderer main thread before sandbox lockdown and before
// workers can query fonts. No path, environment or user-profile overrides.
COMPONENT_EXPORT(CLAWBROWSER_FONT_TYPEFACES)
base::expected<void, std::string> InitializeWindowsFontCatalogBeforeSandbox();
COMPONENT_EXPORT(CLAWBROWSER_FONT_TYPEFACES) sk_sp<SkTypeface> WindowsCatalogFamily(std::string_view family,
                                     const SkFontStyle& style);
COMPONENT_EXPORT(CLAWBROWSER_FONT_TYPEFACES) sk_sp<SkTypeface> WindowsCatalogCharacter(SkUnichar character,
                                        const SkFontStyle& style,
                                        bool emoji);
}  // namespace clawbrowser
#endif
