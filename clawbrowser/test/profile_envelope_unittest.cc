#include "clawbrowser/profile_envelope.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

base::FilePath GetFixturePath(const std::string& filename) {
  base::FilePath src_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root);
  return src_root.AppendASCII("clawbrowser")
      .AppendASCII("test")
      .AppendASCII("fixtures")
      .AppendASCII(filename);
}

TEST(ProfileEnvelopeTest, ParseValidEnvelope) {
  std::string json;
  ASSERT_TRUE(base::ReadFileToString(
      GetFixturePath("valid_fingerprint.json"), &json));

  auto result = ProfileEnvelope::Parse(json);
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_EQ(result->schema_version, 1);
  EXPECT_EQ(result->created_at, "2026-03-23T10:00:00Z");
  EXPECT_EQ(result->request.platform, "macos");
  EXPECT_EQ(result->request.browser, "chrome");
  EXPECT_EQ(result->request.country, "US");
  EXPECT_EQ(result->response.fingerprint.user_agent,
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
            "AppleWebKit/537.36 (KHTML, like Gecko) "
            "Chrome/120.0.0.0 Safari/537.36");
  EXPECT_EQ(result->response.fingerprint.timezone, "America/New_York");
  EXPECT_EQ(result->response.fingerprint.screen.width, 1920);
  EXPECT_EQ(result->response.proxy->host, "proxy.example.com");
}

TEST(ProfileEnvelopeTest, ParseMinimalEnvelope) {
  std::string json;
  ASSERT_TRUE(base::ReadFileToString(
      GetFixturePath("minimal_fingerprint.json"), &json));

  auto result = ProfileEnvelope::Parse(json);
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_EQ(result->schema_version, 1);
  // No proxy in minimal
  EXPECT_FALSE(result->response.proxy.has_value());
  // No optional fields
  EXPECT_TRUE(result->response.fingerprint.media_devices.empty());
  EXPECT_TRUE(result->response.fingerprint.plugins.empty());
}

TEST(ProfileEnvelopeTest, ParseMalformedJson) {
  auto result = ProfileEnvelope::Parse("not json at all");
  ASSERT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("parse"), std::string::npos);
}

TEST(ProfileEnvelopeTest, ParseMissingRequiredField) {
  std::string json;
  ASSERT_TRUE(base::ReadFileToString(
      GetFixturePath("malformed_fingerprint.json"), &json));

  auto result = ProfileEnvelope::Parse(json);
  ASSERT_FALSE(result.has_value());
  // Should report missing required fingerprint fields
}

TEST(ProfileEnvelopeTest, OutdatedSchemaVersionWarns) {
  // schema_version 0 should still parse but set a warning flag
  std::string json = R"({
    "schema_version": 0,
    "created_at": "2026-01-01T00:00:00Z",
    "request": {"platform": "macos", "browser": "chrome", "country": "US"},
    "response": {
      "fingerprint": {
        "user_agent": "test", "platform": "test",
        "screen": {"width": 1, "height": 1, "avail_width": 1, "avail_height": 1, "color_depth": 24, "pixel_ratio": 1.0},
        "hardware": {"concurrency": 1, "memory": 1},
        "webgl": {"vendor": "test", "renderer": "test"},
        "canvas_seed": 1, "audio_seed": 1, "client_rects_seed": 1,
        "timezone": "UTC", "language": ["en"], "fonts": ["Arial"]
      }
    }
  })";
  auto result = ProfileEnvelope::Parse(json);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->schema_outdated);
}

TEST(ProfileEnvelopeTest, MissingSchemaVersionFallsBackToOutdated) {
  std::string json = R"({
    "created_at": "2026-01-01T00:00:00Z",
    "response": {
      "fingerprint": {
        "user_agent": "missing-schema-ua", "platform": "test",
        "screen": {"width": 1, "height": 1, "avail_width": 1, "avail_height": 1, "color_depth": 24, "pixel_ratio": 1.0},
        "hardware": {"concurrency": 1, "memory": 1},
        "webgl": {"vendor": "test", "renderer": "test"},
        "canvas_seed": 1, "audio_seed": 1, "client_rects_seed": 1,
        "timezone": "UTC", "language": ["en"], "fonts": ["Arial"]
      }
    }
  })";

  auto result = ProfileEnvelope::Parse(json);
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_TRUE(result->schema_outdated);
  EXPECT_EQ(result->schema_version, 0);
  EXPECT_EQ(result->response.fingerprint.user_agent, "missing-schema-ua");
  EXPECT_EQ(result->response.fingerprint.timezone, "UTC");
}

TEST(ProfileEnvelopeTest, SerializeRoundTrip) {
  std::string json;
  ASSERT_TRUE(base::ReadFileToString(
      GetFixturePath("valid_fingerprint.json"), &json));

  auto parsed = ProfileEnvelope::Parse(json);
  ASSERT_TRUE(parsed.has_value());

  std::string serialized = parsed->Serialize();
  auto reparsed = ProfileEnvelope::Parse(serialized);
  ASSERT_TRUE(reparsed.has_value());

  EXPECT_EQ(parsed->response.fingerprint.user_agent,
            reparsed->response.fingerprint.user_agent);
  EXPECT_EQ(parsed->response.fingerprint.canvas_seed,
            reparsed->response.fingerprint.canvas_seed);
}

}  // namespace
}  // namespace clawbrowser
