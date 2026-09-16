#include "clawbrowser/font_catalog_typefaces.h"
#include <cstdlib>
#include "base/files/file_util.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkFontArguments.h"
#include "third_party/skia/include/core/SkStream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
TEST(FontCatalogTypefacesTest, ManagedPolicyCannotEscapeEmptyCatalog) {
  for (const auto* family : {"Arial", "serif", "monospace", "Arimo-Regular",
                             "Papyrus", "Segoe UI", ""}) {
    EXPECT_FALSE(MatchManagedCatalogFamily({}, family, SkFontStyle::Normal()));
  }
  EXPECT_FALSE(MatchManagedCatalogCharacter({}, 'A', SkFontStyle::Normal(), false));
  EXPECT_FALSE(MatchManagedCatalogCharacter({}, 0x1f600, SkFontStyle::Normal(), true));
}

TEST(FontCatalogTypefacesTest, RejectsEmptyAndMalformedCatalog) {
  ValidatedFontCatalog catalog;
  EXPECT_FALSE(CreateCatalogTypefaces(catalog).has_value());
  catalog.fonts.push_back({"invalid.ttf", "not a font"});
  EXPECT_FALSE(CreateCatalogTypefaces(catalog).has_value());
}

TEST(FontCatalogTypefacesTest, PinnedCatalogUsesOnlyExplicitBytes) {
  const char* directory = std::getenv("CLAWBROWSER_TEST_CATALOG_DIR");
  const char* digest = std::getenv("CLAWBROWSER_TEST_CATALOG_SHA256");
  if (!directory || !digest) GTEST_SKIP() << "requires pinned QA catalog";
  auto catalog = LoadValidatedFontCatalog(base::FilePath::FromUTF8Unsafe(directory), digest);
  ASSERT_TRUE(catalog.has_value()) << catalog.error();
  auto faces = CreateCatalogTypefaces(*catalog);
  ASSERT_TRUE(faces.has_value()) << faces.error();
  bool emoji = false, cjk_sc = false, arabic = false;
  bool cjk_regular = false, cjk_bold = false, named_collection_face = false;
  for (const auto& face : *faces) {
    EXPECT_GE(face.collection_index, 0);
    EXPECT_GE(face.instance_index, 0);
    if (face.family == "Noto Color Emoji")
      emoji |= face.typeface->unicharToGlyph(0x1f600) != 0;
    if (face.family == "Noto Sans CJK SC") {
      cjk_sc |= face.typeface->unicharToGlyph(0x4e2d) != 0;
      cjk_regular |= face.style.weight() == 400;
      cjk_bold |= face.style.weight() == 700;
      named_collection_face |= face.collection_index > 0 && face.instance_index > 0;
    }
    if (face.family == "DejaVu Sans")
      arabic |= face.typeface->unicharToGlyph(0x0627) != 0;
  }
  EXPECT_TRUE(emoji);
  EXPECT_TRUE(cjk_sc);
  EXPECT_TRUE(arabic);
  EXPECT_TRUE(cjk_regular);
  EXPECT_TRUE(cjk_bold);
  EXPECT_TRUE(named_collection_face);
  auto regular = MatchCatalogFamily(*faces, "aRiMo", SkFontStyle::Normal());
  ASSERT_TRUE(regular);
  EXPECT_EQ(regular->fontStyle().weight(), 400);
  auto bold = MatchCatalogFamily(*faces, "Arimo", SkFontStyle::Bold());
  ASSERT_TRUE(bold);
  EXPECT_EQ(bold->fontStyle().weight(), 700);
  auto italic = MatchCatalogFamily(*faces, "Arimo", SkFontStyle::Italic());
  ASSERT_TRUE(italic);
  EXPECT_EQ(italic->fontStyle().slant(), SkFontStyle::kItalic_Slant);
  EXPECT_FALSE(MatchCatalogFamily(*faces, "Papyrus", SkFontStyle::Normal()));
  auto cjk = MatchCatalogFamily(*faces, "Noto Sans CJK SC", SkFontStyle::Normal());
  ASSERT_TRUE(cjk);
  int collection_index = 0;
  auto collection_stream = cjk->openStream(&collection_index);
  ASSERT_TRUE(collection_stream);
  ASSERT_GT(collection_index, 0);
  const SkFontArguments::VariationPosition::Coordinate weight = {
      SkSetFourByteTag('w', 'g', 'h', 't'), 675};
  SkFontArguments clone_args;
  clone_args.setCollectionIndex(collection_index).setVariationDesignPosition({&weight, 1});
  auto clone = cjk->makeClone(clone_args);
  ASSERT_TRUE(clone);
  SkString clone_family;
  clone->getFamilyName(&clone_family);
  EXPECT_EQ(clone_family, SkString("Noto Sans CJK SC"));
  EXPECT_EQ(clone->fontStyle().weight(), 675);
  auto fallback = MatchCatalogCharacter(*faces, {"Arimo", "Noto Color Emoji"},
                                       SkFontStyle::Normal(), 0x1f600);
  ASSERT_TRUE(fallback);
  SkString family;
  fallback->getFamilyName(&family);
  EXPECT_EQ(family, SkString("Noto Color Emoji"));
  auto first = MatchCatalogCharacter(*faces, {"Tinos", "Arimo"},
                                    SkFontStyle::Normal(), 'A');
  ASSERT_TRUE(first);
  first->getFamilyName(&family);
  EXPECT_EQ(family, SkString("Tinos"));
  EXPECT_FALSE(MatchCatalogCharacter(*faces, {"Arimo"}, SkFontStyle::Normal(), 0x1f600));
  EXPECT_FALSE(MatchCatalogCharacter(*faces, {"DejaVu Sans"}, SkFontStyle::Normal(), 0xd800));
  EXPECT_FALSE(MatchCatalogCharacter(*faces, {"DejaVu Sans"}, SkFontStyle::Normal(), 0x110000));
  // Exercise the exact shared policy used by the macOS adapter, and reusable
  // by Windows, against real checked font bytes rather than a mock font mgr.
  for (const auto* name : {"sans", "sans-serif", "Arial", "HELVeTICA", "system-ui",
                           "-apple-system", "BlinkMacSystemFont", ".AppleSystemUIFont"}) {
    auto matched = MatchManagedCatalogFamily(*faces, name, SkFontStyle::BoldItalic());
    ASSERT_TRUE(matched) << name;
    matched->getFamilyName(&family);
    EXPECT_EQ(family, SkString("Arimo")) << name;
    EXPECT_EQ(matched->fontStyle().weight(), 700) << name;
    EXPECT_EQ(matched->fontStyle().slant(), SkFontStyle::kItalic_Slant) << name;
  }
  for (const auto* name : {"serif", "Times", "Times New Roman"}) {
    auto matched = MatchManagedCatalogFamily(*faces, name, SkFontStyle::Normal());
    ASSERT_TRUE(matched) << name;
    matched->getFamilyName(&family);
    EXPECT_EQ(family, SkString("Tinos")) << name;
  }
  for (const auto* name : {"monospace", "Courier", "Courier New"}) {
    auto matched = MatchManagedCatalogFamily(*faces, name, SkFontStyle::Normal());
    ASSERT_TRUE(matched) << name;
    matched->getFamilyName(&family);
    EXPECT_EQ(family, SkString("Cousine")) << name;
  }
  // A named face selects its own style, not the surrounding CSS request.
  auto named_regular = MatchManagedCatalogFamily(*faces, "Arimo-Regular", SkFontStyle::BoldItalic());
  ASSERT_TRUE(named_regular);
  EXPECT_EQ(named_regular->fontStyle(), SkFontStyle::Normal());
  auto named_bold_italic = MatchManagedCatalogFamily(*faces, "Arimo Bold Italic", SkFontStyle::Normal());
  ASSERT_TRUE(named_bold_italic);
  EXPECT_EQ(named_bold_italic->fontStyle(), SkFontStyle::BoldItalic());
  for (const auto* name : {"Arimo-Regular-extra", "Arial Narrow", "Segoe UI", "Papyrus", ""})
    EXPECT_FALSE(MatchManagedCatalogFamily(*faces, name, SkFontStyle::Normal())) << name;
  auto managed_emoji = MatchManagedCatalogCharacter(*faces, 0x1f600, SkFontStyle::Normal(), true);
  ASSERT_TRUE(managed_emoji);
  managed_emoji->getFamilyName(&family);
  EXPECT_EQ(family, SkString("Noto Color Emoji"));
  auto managed_latin = MatchManagedCatalogCharacter(*faces, 'A', SkFontStyle::Normal(), false);
  ASSERT_TRUE(managed_latin);
  managed_latin->getFamilyName(&family);
  EXPECT_EQ(family, SkString("Arimo"));
  EXPECT_FALSE(MatchManagedCatalogCharacter(*faces, 0xd800, SkFontStyle::Normal(), false));
  EXPECT_FALSE(MatchManagedCatalogCharacter(*faces, 0x110000, SkFontStyle::Normal(), true));
  RecordProperty("catalog_face_count", static_cast<int>(faces->size()));
}
}  // namespace clawbrowser
