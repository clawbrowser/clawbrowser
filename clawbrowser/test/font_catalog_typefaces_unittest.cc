#include "clawbrowser/font_catalog_typefaces.h"
#include <cstdlib>
#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
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
  RecordProperty("catalog_face_count", static_cast<int>(faces->size()));
}
}  // namespace clawbrowser
