#include "clawbrowser/font_catalog_typefaces.h"

#include "base/strings/string_util.h"
#include "clawbrowser/font_catalog_identity.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/ports/SkFontScanner_Fontations.h"
#include "third_party/skia/include/ports/SkTypeface_fontations.h"

namespace clawbrowser {
namespace {
class CatalogStyleSet final : public SkFontStyleSet {
 public:
  std::vector<const CatalogTypeface*> faces;
  int count() override { return static_cast<int>(faces.size()); }
  void getStyle(int index, SkFontStyle* style, SkString* name) override {
    if (style) *style = faces[index]->style;
    if (name) name->reset();
  }
  sk_sp<SkTypeface> createTypeface(int index) override {
    return faces[index]->typeface;
  }
  sk_sp<SkTypeface> matchStyle(const SkFontStyle& style) override {
    return faces.empty() ? nullptr : matchStyleCSS3(style);
  }
};
}  // namespace

sk_sp<SkTypeface> MatchCatalogFamily(
    const std::vector<CatalogTypeface>& faces,
    std::string_view family,
    const SkFontStyle& style) {
  CatalogStyleSet choices;
  for (const auto& face : faces) {
    if (face.typeface && base::EqualsCaseInsensitiveASCII(face.family, family))
      choices.faces.push_back(&face);
  }
  return choices.matchStyle(style);
}

sk_sp<SkTypeface> MatchCatalogCharacter(
    const std::vector<CatalogTypeface>& faces,
    const std::vector<std::string>& families,
    const SkFontStyle& style,
    SkUnichar character) {
  if (character < 0 || character > 0x10ffff ||
      (character >= 0xd800 && character <= 0xdfff))
    return nullptr;
  for (const auto& family : families) {
    CatalogStyleSet choices;
    for (const auto& face : faces) {
      if (face.typeface && base::EqualsCaseInsensitiveASCII(face.family, family) &&
          face.typeface->unicharToGlyph(character))
        choices.faces.push_back(&face);
    }
    if (auto result = choices.matchStyle(style)) return result;
  }
  return nullptr;
}

sk_sp<SkTypeface> MatchManagedCatalogFamily(
    const std::vector<CatalogTypeface>& faces,
    std::string_view family,
    const SkFontStyle& style) {
  const auto alias = LinuxFontAliasFamily(family);
  if (!alias.empty()) {
    const auto lower = base::ToLowerASCII(family);
    const bool bold = lower.find("bold") != std::string::npos;
    const bool italic = lower.find("italic") != std::string::npos;
    return MatchCatalogFamily(
        faces, alias,
        SkFontStyle(bold ? 700 : 400, SkFontStyle::kNormal_Width,
                    italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant));
  }
  const auto name = base::ToLowerASCII(family);
  if (name == "serif" || name == "times" || name == "times new roman")
    return MatchCatalogFamily(faces, "Tinos", style);
  if (name == "monospace" || name == "courier" || name == "courier new")
    return MatchCatalogFamily(faces, "Cousine", style);
  if (name == "sans" || name == "sans-serif" || name == "arial" ||
      name == "helvetica" || name == "system-ui" || name == "-apple-system" ||
      name == "blinkmacsystemfont" || name == ".applesystemuifont")
    return MatchCatalogFamily(faces, "Arimo", style);
  return MatchCatalogFamily(faces, family, style);
}

sk_sp<SkTypeface> MatchManagedCatalogCharacter(
    const std::vector<CatalogTypeface>& faces,
    SkUnichar character,
    const SkFontStyle& style,
    bool emoji) {
  auto families = LinuxFontCatalogFamilies();
  if (emoji) families.insert(families.begin(), "Noto Color Emoji");
  return MatchCatalogCharacter(faces, families, style, character);
}

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
