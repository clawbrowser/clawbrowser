#include "clawbrowser/managed_font_rendering.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {

TEST(ManagedFontRenderingTest, HostStrikePreferencesCannotChangeManagedPolicy) {
  for (bool geometric : {false, true}) {
    for (int flags = 0; flags < 16; ++flags) {
      for (auto edging : {SkFont::Edging::kAlias, SkFont::Edging::kAntiAlias,
                          SkFont::Edging::kSubpixelAntiAlias}) {
        for (auto hinting : {SkFontHinting::kNone, SkFontHinting::kSlight,
                             SkFontHinting::kNormal, SkFontHinting::kFull}) {
          SkFont font;
          font.setSubpixel(flags & 1);
          font.setLinearMetrics(flags & 2);
          font.setEmbeddedBitmaps(flags & 4);
          font.setForceAutoHinting(flags & 8);
          font.setEdging(edging);
          font.setHinting(hinting);
          ApplyManagedFontRendering(font, geometric);
          EXPECT_TRUE(font.isSubpixel());
          EXPECT_TRUE(font.isLinearMetrics());
          EXPECT_FALSE(font.isEmbeddedBitmaps());
          EXPECT_FALSE(font.isForceAutoHinting());
          EXPECT_EQ(font.getEdging(), SkFont::Edging::kSubpixelAntiAlias);
          EXPECT_EQ(font.getHinting(), geometric ? SkFontHinting::kNone
                                                : SkFontHinting::kNormal);
        }
      }
    }
  }
}

TEST(ManagedFontRenderingTest, PreservesSizeAndSyntheticStyle) {
  SkFont font;
  font.setSize(17.25f);
  font.setScaleX(1.125f);
  font.setSkewX(-0.25f);
  font.setEmbolden(true);
  const auto* typeface = font.getTypeface();
  for (bool geometric : {false, true}) {
    ApplyManagedFontRendering(font, geometric);
    EXPECT_EQ(font.getTypeface(), typeface);
    EXPECT_EQ(font.getSize(), 17.25f);
    EXPECT_EQ(font.getScaleX(), 1.125f);
    EXPECT_EQ(font.getSkewX(), -0.25f);
    EXPECT_TRUE(font.isEmbolden());
  }
}

}  // namespace clawbrowser
