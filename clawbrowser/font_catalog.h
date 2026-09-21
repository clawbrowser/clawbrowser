#ifndef CLAWBROWSER_FONT_CATALOG_H_
#define CLAWBROWSER_FONT_CATALOG_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/types/expected.h"

namespace clawbrowser {

struct ValidatedFontAsset {
  std::string file_name;
  std::string bytes;
};

struct ValidatedFontCatalog {
  std::string catalog_id;
  std::string manifest_json;
  std::vector<ValidatedFontAsset> fonts;
};

// Returns the exact bytes checked against the pinned manifest. Consumers must
// instantiate fonts from this snapshot, not reopen paths after validation.
// This does not register fonts or alter system fallback selection.
base::expected<ValidatedFontCatalog, std::string> LoadValidatedFontCatalog(
    const base::FilePath& catalog_dir,
    std::string_view expected_manifest_sha256);

// Validate an installed closed catalog against a build-pinned manifest digest.
// Does not change the process font manager or environment. The installation
// directory must be trusted/immutable for the lifetime of browser processes.
base::expected<std::string, std::string> ValidateFontCatalog(
    const base::FilePath& catalog_dir,
    std::string_view expected_manifest_sha256);

// Constructs closed Fontconfig XML from the pinned manifest, never from a
// caller-supplied fonts.conf. Both paths must be absolute.
base::expected<std::string, std::string> BuildFontCatalogConfig(
    const base::FilePath& catalog_dir,
    const base::FilePath& cache_dir,
    std::string_view expected_manifest_sha256);

}  // namespace clawbrowser
#endif
