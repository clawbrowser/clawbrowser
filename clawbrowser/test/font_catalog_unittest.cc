#include "clawbrowser/font_catalog.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "crypto/hash.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {
std::string Hash(std::string_view bytes) {
  return base::ToLowerASCII(base::HexEncode(crypto::hash::Sha256(bytes)));
}

class FontCatalogTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateDirectory(dir_.GetPath().AppendASCII("fonts")));
    ASSERT_TRUE(base::WriteFile(Font(), "fixture bytes"));
    manifest_ = "{\"catalog_id\":\"test-v1\",\"fonts\":[{\"file\":\"Test.ttf\","
                "\"sha256\":\"" + Hash("fixture bytes") + "\"}],"
                "\"generics\":{\"serif\":\"Test & Family\"},"
                "\"fallback\":[\"Test & Family\"]}";
    ASSERT_TRUE(base::WriteFile(dir_.GetPath().AppendASCII("manifest.json"), manifest_));
  }
  base::FilePath Font() { return dir_.GetPath().AppendASCII("fonts/Test.ttf"); }
  base::ScopedTempDir dir_;
  std::string manifest_;
};

TEST_F(FontCatalogTest, AcceptsExactPinnedFiles) {
  auto result = ValidateFontCatalog(dir_.GetPath(), Hash(manifest_));
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(*result, "test-v1");
}

TEST_F(FontCatalogTest, RejectsWrongManifestDigest) {
  EXPECT_FALSE(ValidateFontCatalog(dir_.GetPath(), std::string(64, '0')).has_value());
}

TEST_F(FontCatalogTest, RejectsCorruptFont) {
  ASSERT_TRUE(base::WriteFile(Font(), "changed"));
  EXPECT_FALSE(ValidateFontCatalog(dir_.GetPath(), Hash(manifest_)).has_value());
}

TEST_F(FontCatalogTest, RejectsMissingFont) {
  ASSERT_TRUE(base::DeleteFile(Font()));
  EXPECT_FALSE(ValidateFontCatalog(dir_.GetPath(), Hash(manifest_)).has_value());
}

TEST_F(FontCatalogTest, RejectsUnlistedFont) {
  ASSERT_TRUE(base::WriteFile(dir_.GetPath().AppendASCII("fonts/Extra.ttf"), "extra"));
  EXPECT_FALSE(ValidateFontCatalog(dir_.GetPath(), Hash(manifest_)).has_value());
}

TEST_F(FontCatalogTest, BuildsClosedConfigWithEscapedText) {
  auto result = BuildFontCatalogConfig(
      dir_.GetPath(), dir_.GetPath().AppendASCII("cache<&"), Hash(manifest_));
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_NE(result->find("<reset-dirs/>"), std::string::npos);
  EXPECT_EQ(result->find("<include"), std::string::npos);
  EXPECT_NE(result->find("Test &amp; Family"), std::string::npos);
  EXPECT_NE(result->find("cache&lt;&amp;"), std::string::npos);
}

TEST_F(FontCatalogTest, RejectsRelativeConfigPaths) {
  EXPECT_FALSE(BuildFontCatalogConfig(
      dir_.GetPath(), base::FilePath(FILE_PATH_LITERAL("relative")),
      Hash(manifest_)).has_value());
}

TEST_F(FontCatalogTest, DoesNotUseExternalFontconfigFile) {
  ASSERT_TRUE(base::WriteFile(dir_.GetPath().AppendASCII("fonts.conf"),
                             "<fontconfig><dir>/host/fonts</dir></fontconfig>"));
  auto result = BuildFontCatalogConfig(
      dir_.GetPath(), dir_.GetPath().AppendASCII("cache"), Hash(manifest_));
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(result->find("/host/fonts"), std::string::npos);
}
}  // namespace
}  // namespace clawbrowser
