#include "clawbrowser/windows_font_catalog.h"

#include <optional>
#include <utility>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "font_catalog_build.h"

namespace clawbrowser {
namespace {
std::optional<std::vector<CatalogTypeface>>& CatalogStorage() {
  static base::NoDestructor<std::optional<std::vector<CatalogTypeface>>> faces;
  return *faces;
}

const std::vector<CatalogTypeface>& Faces() {
  // Deliberately no lazy filesystem load inside a sandboxed lookup and no
  // native fallback if the renderer initialization hook was omitted.
  CHECK(CatalogStorage().has_value()) << "Managed font catalog not initialized";
  return *CatalogStorage();
}
}  // namespace

base::expected<void, std::string> InitializeWindowsFontCatalogBeforeSandbox() {
  if (CatalogStorage().has_value())
    return base::ok();
  base::FilePath module_dir;
  // The installed chrome.dll is versioned while the launcher may live in its
  // parent. Resources must track the loaded module, not the launcher or cwd.
  if (!base::PathService::Get(base::DIR_MODULE, &module_dir))
    return base::unexpected("cannot locate managed font resource module");
  auto loaded = LoadCatalogTypefaces(module_dir.AppendASCII(kFontCatalogDirectory),
                                    kFontCatalogManifestHash);
  if (!loaded.has_value())
    return base::unexpected(loaded.error());
  CatalogStorage().emplace(std::move(*loaded));
  return base::ok();
}

sk_sp<SkTypeface> WindowsCatalogFamily(std::string_view family,
                                     const SkFontStyle& style) {
  return MatchManagedCatalogFamily(Faces(), family, style);
}

sk_sp<SkTypeface> WindowsCatalogCharacter(SkUnichar character,
                                        const SkFontStyle& style,
                                        bool emoji) {
  return MatchManagedCatalogCharacter(Faces(), character, style, emoji);
}
}  // namespace clawbrowser
