#ifndef CLAWBROWSER_WINDOWS_INSTALLED_FONTS_H_
#define CLAWBROWSER_WINDOWS_INSTALLED_FONTS_H_

#include <optional>
#include <string>
#include <vector>

namespace clawbrowser {

// English family names of the host's DirectWrite system font collection, the
// same collection the renderer's font proxy resolves. Browser process only.
// Returns std::nullopt when DirectWrite cannot be queried, so callers keep
// the unfiltered list instead of treating the host as having no fonts.
std::optional<std::vector<std::string>> GetInstalledFontFamilies();

}  // namespace clawbrowser

#endif  // CLAWBROWSER_WINDOWS_INSTALLED_FONTS_H_
