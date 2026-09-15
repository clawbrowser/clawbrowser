#include "clawbrowser/font_catalog_typefaces.h"

#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/ports/SkFontScanner_Fontations.h"
#include "third_party/skia/include/ports/SkTypeface_fontations.h"

namespace clawbrowser {
base::expected<std::vector<CatalogTypeface>, std::string>
CreateCatalogTypefaces(const ValidatedFontCatalog& catalog) {
  if (catalog.fonts.empty())
    return base::unexpected("cannot instantiate empty font catalog");
  auto scanner = SkFontScanner_Make_Fontations();
  std::vector<CatalogTypeface> result;
  for (const auto& asset : catalog.fonts) {
    auto data = SkData::MakeWithCopy(asset.bytes.data(), asset.bytes.size());
    SkMemoryStream stream(data);
    int face_count = 0;
    if (!scanner->scanFile(&stream, &face_count) || face_count <= 0 ||
        face_count > 256)
      return base::unexpected("cannot scan catalog font: " + asset.file_name);
    for (int face = 0; face < face_count; ++face) {
      int instance_count = 0;
      if (!scanner->scanFace(&stream, face, &instance_count) ||
          instance_count < 0 || instance_count > 1024)
        return base::unexpected("cannot scan catalog collection face");
      for (int instance = 0; instance <= instance_count; ++instance) {
        if (result.size() >= 4096)
          return base::unexpected("catalog face budget exceeded");
        SkFontArguments args;
        // SkTypeface_Fontations decodes the low bits as the collection face
        // and high bits as the named-instance index, then applies its axes.
        // Unlike scanInstance in this pinned Skia, it masks the collection
        // index before opening the font reference.
        args.setCollectionIndex(face | (instance << 16));
        auto typeface = SkTypeface_Make_Fontations(data, args);
        if (!typeface)
          return base::unexpected("cannot instantiate catalog font instance");
        SkString family;
        typeface->getFamilyName(&family);
        if (family.isEmpty())
          return base::unexpected("catalog face has no family name");
        const auto style = typeface->fontStyle();
        result.push_back({family.c_str(), style, face, instance,
                          std::move(typeface)});
      }
    }
  }
  return base::ok(std::move(result));
}
}  // namespace clawbrowser
