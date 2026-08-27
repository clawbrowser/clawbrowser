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

GenerateResponse MakeResponseWithProxy(const std::string& username,
                                      const std::string& password) {
  GenerateResponse response;
  response.fingerprint.user_agent = "test-ua";
  response.fingerprint.platform = "test-platform";
  response.fingerprint.screen.width = 1920;
  response.fingerprint.screen.height = 1080;
  response.fingerprint.screen.avail_width = 1920;
  response.fingerprint.screen.avail_height = 1040;
  response.fingerprint.screen.color_depth = 24;
  response.fingerprint.screen.pixel_ratio = 1.0;
  response.fingerprint.hardware.concurrency = 8;
  response.fingerprint.hardware.memory = 8;
  response.fingerprint.webgl.vendor = "vendor";
  response.fingerprint.webgl.renderer = "renderer";
  response.fingerprint.canvas_seed = 1;
  response.fingerprint.audio_seed = 2;
  response.fingerprint.client_rects_seed = 3;
  response.fingerprint.timezone = "UTC";
  response.fingerprint.language = {"en-US"};
  response.fingerprint.fonts = {"Arial"};
  response.proxy.emplace();
  response.proxy->scheme = "http";
  response.proxy->host = "proxy.example.com";
  response.proxy->port = 3128;
  response.proxy->country = "US";
  response.proxy->city = "New York";
  response.proxy->username = username;
  response.proxy->password = password;
  return response;
}

TEST(ProfileEnvelopeTest, ParseValidEnvelope) {
  std::string json;
  ASSERT_TRUE(base::ReadFileToString(
      GetFixturePath("valid_fingerprint.json"), &json));

  auto result = ProfileEnvelope::Parse(json);
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_EQ(result->schema_version, 2);
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

  EXPECT_EQ(result->schema_version, 2);
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
    "request": {"platform": "macos", "browser": "clawbrowser", "country": "US"},
    "response": {
      "fingerprint": {
        "browser_family": "chrome",
        "browser_version": "120.0.0.0",
        "engine": "blink",
        "os": "macos",
        "os_version": "10.15.7",
        "architecture": "arm64",
        "device_class": "desktop",
        "user_agent_data": {"brands": [{"brand": "Chromium", "version": "120"}], "fullVersionList": [{"brand": "Chromium", "version": "120.0.0.0"}], "platform": "macOS", "platformVersion": "10.15.7", "architecture": "arm", "bitness": "64", "mobile": false, "model": ""},
        "headers": {"Accept-Language": "en-US"},
        "surface_policy": {"canvas": {"mode": "native"}, "audio": {"mode": "native"}, "client_rects": {"mode": "native"}, "webgl": {"mode": "native"}, "fonts": {"mode": "native_or_allowlist"}, "plugins": {"mode": "override"}, "media_devices": {"mode": "override"}, "speech_voices": {"mode": "override"}},
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
        "browser_family": "chrome",
        "browser_version": "120.0.0.0",
        "engine": "blink",
        "os": "macos",
        "os_version": "10.15.7",
        "architecture": "arm64",
        "device_class": "desktop",
        "user_agent_data": {"brands": [{"brand": "Chromium", "version": "120"}], "fullVersionList": [{"brand": "Chromium", "version": "120.0.0.0"}], "platform": "macOS", "platformVersion": "10.15.7", "architecture": "arm", "bitness": "64", "mobile": false, "model": ""},
        "headers": {"Accept-Language": "en-US"},
        "surface_policy": {"canvas": {"mode": "native"}, "audio": {"mode": "native"}, "client_rects": {"mode": "native"}, "webgl": {"mode": "native"}, "fonts": {"mode": "native_or_allowlist"}, "plugins": {"mode": "override"}, "media_devices": {"mode": "override"}, "speech_voices": {"mode": "override"}},
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

TEST(ProfileEnvelopeTest, SerializeEncryptsProxyCredentials) {
  ProfileEnvelope envelope;
  envelope.schema_version = ProfileEnvelope::kCurrentSchemaVersion;
  envelope.created_at = "2026-04-09T12:00:00Z";
  envelope.request.platform = "macos";
  envelope.request.browser = "chrome";
  envelope.request.country = "US";
  envelope.response = MakeResponseWithProxy("user_abc", "pass_xyz");

  std::string serialized = envelope.Serialize();
  EXPECT_EQ(serialized.find("user_abc"), std::string::npos);
  EXPECT_EQ(serialized.find("pass_xyz"), std::string::npos);
  EXPECT_NE(serialized.find("encrypted_proxy_credentials"),
            std::string::npos);

  auto reparsed = ProfileEnvelope::Parse(serialized);
  ASSERT_TRUE(reparsed.has_value()) << reparsed.error();
  ASSERT_TRUE(reparsed->response.proxy.has_value());
  EXPECT_EQ(reparsed->schema_version, ProfileEnvelope::kCurrentSchemaVersion);
  EXPECT_EQ(reparsed->response.proxy->host.value_or(""), "proxy.example.com");
  EXPECT_EQ(reparsed->response.proxy->username.value_or(""), "user_abc");
  EXPECT_EQ(reparsed->response.proxy->password.value_or(""), "pass_xyz");
}

TEST(ProfileEnvelopeTest, SerializePreservesProxySchemeWithoutPlaintextSecrets) {
  ProfileEnvelope envelope;
  envelope.schema_version = ProfileEnvelope::kCurrentSchemaVersion;
  envelope.created_at = "2026-04-09T12:00:00Z";
  envelope.request.platform = "macos";
  envelope.request.browser = "chrome";
  envelope.request.country = "US";
  envelope.response = MakeResponseWithProxy("user_abc", "pass_xyz");
  envelope.response.proxy->scheme = "socks5";

  std::string serialized = envelope.Serialize();
  EXPECT_NE(serialized.find("\"scheme\": \"socks5\""), std::string::npos);
  EXPECT_EQ(serialized.find("user_abc"), std::string::npos);
  EXPECT_EQ(serialized.find("pass_xyz"), std::string::npos);

  auto reparsed = ProfileEnvelope::Parse(serialized);
  ASSERT_TRUE(reparsed.has_value()) << reparsed.error();
  ASSERT_TRUE(reparsed->response.proxy.has_value());
  EXPECT_EQ(reparsed->response.proxy->scheme.value_or(""), "socks5");
  EXPECT_EQ(reparsed->response.proxy->username.value_or(""), "user_abc");
  EXPECT_EQ(reparsed->response.proxy->password.value_or(""), "pass_xyz");
}

}  // namespace
}  // namespace clawbrowser
