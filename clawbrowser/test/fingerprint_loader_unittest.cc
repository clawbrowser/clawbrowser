#include "clawbrowser/fingerprint_loader.h"

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "clawbrowser/cli/args.h"
#include "clawbrowser/fingerprint_accessor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "clawbrowser/profile_envelope.h"

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

class FingerprintLoaderTest : public testing::Test {
 protected:
  void TearDown() override {
    // Reset global accessor state between tests
    FingerprintAccessor::Reset();
  }
};

TEST_F(FingerprintLoaderTest, LoadValidFile) {
  base::FilePath path = GetFixturePath("valid_fingerprint.json");
  auto result = LoadFingerprint(path);
  ASSERT_TRUE(result.has_value()) << result.error();

  // Accessor should now return data
  const auto* fp = FingerprintAccessor::Get();
  ASSERT_NE(fp, nullptr);
  EXPECT_EQ(fp->user_agent,
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
            "AppleWebKit/537.36 (KHTML, like Gecko) "
            "Chrome/120.0.0.0 Safari/537.36");
  EXPECT_EQ(fp->screen.width, 1920);
  EXPECT_EQ(fp->canvas_seed, 1234567890);
}

TEST_F(FingerprintLoaderTest, LoadMinimalFile) {
  base::FilePath path = GetFixturePath("minimal_fingerprint.json");
  auto result = LoadFingerprint(path);
  ASSERT_TRUE(result.has_value()) << result.error();

  const auto* fp = FingerprintAccessor::Get();
  ASSERT_NE(fp, nullptr);
  // Optional fields should be absent/empty
  EXPECT_TRUE(fp->media_devices.empty());
}

TEST_F(FingerprintLoaderTest, LoadEnvelopeWithoutSchemaVersion) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath path = temp_dir.GetPath().AppendASCII("fingerprint.json");
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
  ASSERT_TRUE(base::WriteFile(path, json));

  testing::internal::CaptureStderr();
  auto result = LoadFingerprint(path);
  std::string stderr_output = testing::internal::GetCapturedStderr();
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_NE(stderr_output.find("warn: fingerprint schema version"),
            std::string::npos);

  const auto* fp = FingerprintAccessor::Get();
  ASSERT_NE(fp, nullptr);
  EXPECT_EQ(fp->user_agent, "missing-schema-ua");
}

TEST_F(FingerprintLoaderTest, CurrentSchemaRejectsIncompleteDynamicSurfaceData) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath path =
      temp_dir.GetPath().AppendASCII("incomplete_fingerprint.json");
  std::string json = R"({
    "schema_version": 2,
    "created_at": "2026-01-01T00:00:00Z",
    "response": {
      "fingerprint": {
        "browser_family": "chrome",
        "browser_version": "148.0.7769.0",
        "engine": "blink",
        "os": "macos",
        "os_version": "10.15.7",
        "architecture": "arm64",
        "device_class": "desktop",
        "user_agent": "incomplete-ua",
        "platform": "MacIntel",
        "screen": {"width": 1, "height": 1, "avail_width": 1, "avail_height": 1, "color_depth": 24, "pixel_ratio": 1.0},
        "hardware": {"concurrency": 1, "memory": 1},
        "webgl": {"vendor": "backend-vendor", "renderer": "backend-renderer"},
        "canvas_seed": 1, "audio_seed": 1, "client_rects_seed": 1,
        "timezone": "UTC", "language": ["en"], "fonts": ["Arial"]
      }
    }
  })";
  ASSERT_TRUE(base::WriteFile(path, json));

  auto result = LoadFingerprint(path);
  ASSERT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("user_agent_data"), std::string::npos);
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(FingerprintLoaderTest, LoadMissingFile) {
  base::FilePath path(FILE_PATH_LITERAL("/nonexistent/path.json"));
  auto result = LoadFingerprint(path);
  ASSERT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("read"), std::string::npos);
}

TEST_F(FingerprintLoaderTest, LoadMalformedFile) {
  base::FilePath path = GetFixturePath("malformed_fingerprint.json");
  auto result = LoadFingerprint(path);
  ASSERT_FALSE(result.has_value());
}

TEST_F(FingerprintLoaderTest, RejectsFingerprintValuesThatWouldLeakHostScreen) {
  std::string json;
  ASSERT_TRUE(base::ReadFileToString(GetFixturePath("valid_fingerprint.json"),
                                    &json));
  auto parsed = ProfileEnvelope::Parse(json);
  ASSERT_TRUE(parsed.has_value()) << parsed.error();
  const Fingerprint valid = parsed->response.fingerprint;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  int case_index = 0;
  const auto expect_rejected = [&](Fingerprint fingerprint,
                                   const std::string& expected_error) {
    ProfileEnvelope envelope = *parsed;
    envelope.response.fingerprint = std::move(fingerprint);
    const base::FilePath path = temp_dir.GetPath().AppendASCII(
        "invalid-" + std::to_string(case_index++) + ".json");
    ASSERT_TRUE(base::WriteFile(path, envelope.Serialize()));

    auto result = LoadFingerprint(path);
    EXPECT_FALSE(result.has_value());
    if (!result.has_value()) {
      EXPECT_NE(result.error().find(expected_error), std::string::npos)
          << result.error();
    }
    EXPECT_EQ(FingerprintAccessor::Get(), nullptr);

    auto child_payload = BuildChildFingerprintPayload(path);
    EXPECT_FALSE(child_payload.has_value());
    if (!child_payload.has_value()) {
      EXPECT_NE(child_payload.error().find(expected_error), std::string::npos)
          << child_payload.error();
    }
  };

  Fingerprint invalid = valid;
  invalid.screen.width = 0;
  expect_rejected(invalid, "screen dimensions must be positive");

  invalid = valid;
  invalid.screen.avail_width = invalid.screen.width + 1;
  expect_rejected(invalid, "available screen exceeds full screen");

  invalid = valid;
  invalid.screen.pixel_ratio = 0.0;
  expect_rejected(invalid, "screen pixel ratio must be finite and in");

  invalid = valid;
  invalid.screen.pixel_ratio = 10.5;
  expect_rejected(invalid, "screen pixel ratio must be finite and in");

  invalid = valid;
  invalid.screen.color_depth = 0;
  expect_rejected(invalid, "screen color depth must be 3-48 bits");

  invalid = valid;
  invalid.screen.color_depth = 25;
  expect_rejected(invalid, "screen color depth must be 3-48 bits");

  invalid = valid;
  invalid.screen.width = 32769;
  expect_rejected(invalid, "screen dimensions exceed runtime limits");

  invalid = valid;
  invalid.fonts.clear();
  invalid.surface_policy.fonts.mode = "native_or_allowlist";
  expect_rejected(invalid, "protected font allowlist is empty");

  invalid = valid;
  invalid.fonts = {"   \t"};
  invalid.surface_policy.fonts.mode = "native_or_allowlist";
  expect_rejected(invalid,
                  "protected font allowlist contains an empty name");
}

TEST_F(FingerprintLoaderTest, AccessorReturnsNullWhenNotLoaded) {
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(FingerprintLoaderTest, AccessorReturnsNullAfterReset) {
  base::FilePath path = GetFixturePath("valid_fingerprint.json");
  auto result = LoadFingerprint(path);
  ASSERT_TRUE(result.has_value());
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);

  FingerprintAccessor::Reset();
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(FingerprintLoaderTest, LoadFromCommandLine) {
  base::FilePath path = GetFixturePath("valid_fingerprint.json");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchPath(kFingerprintPathSwitch, path);

  auto result = LoadFingerprintFromCommandLine(cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
}

TEST_F(FingerprintLoaderTest, InlineCommandLineDataIsIgnored) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("clawbrowser-fp-data", "ZmFrZQ==");

  auto result = LoadFingerprintFromCommandLine(cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(FingerprintLoaderTest, ManagedChildWithoutFingerprintDataFails) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitch(kRequireFingerprintSwitch);

  auto result = LoadFingerprintFromCommandLine(cmd);

  ASSERT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("missing fingerprint payload and path"),
            std::string::npos);
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(FingerprintLoaderTest, ManagedChildWithUnreadablePathFails) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitch(kRequireFingerprintSwitch);
  cmd.AppendSwitchPath(kFingerprintPathSwitch,
                       temp_dir.GetPath().AppendASCII("missing-profile.json"));

  auto result = LoadFingerprintFromCommandLine(cmd);

  ASSERT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("failed to read fingerprint file"),
            std::string::npos);
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(FingerprintLoaderTest, ManagedChildWithCorruptPayloadFails) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitch(kRequireFingerprintSwitch);
  cmd.AppendSwitchASCII(kFingerprintChildDataSwitch, "not-base64!");

  auto result = LoadFingerprintFromCommandLine(cmd);

  ASSERT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("decode child fingerprint payload"),
            std::string::npos);
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(FingerprintLoaderTest, LoadFromChildPayloadPrefersChildData) {
  const std::string child_json = R"({
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
      "user_agent": "child-ua",
      "platform": "child-platform",
      "screen": {
        "width": 1,
        "height": 2,
        "avail_width": 1,
        "avail_height": 2,
        "color_depth": 24,
        "pixel_ratio": 1.25
      },
      "hardware": {
        "concurrency": 5,
        "memory": 6
      },
      "webgl": {
        "vendor": "child-vendor",
        "renderer": "child-renderer"
      },
      "canvas_seed": 7,
      "audio_seed": 8,
      "client_rects_seed": 9,
      "timezone": "Etc/GMT-14",
      "language": ["en-US"],
      "fonts": ["Example Font"]
    }
  })";

  std::string child_payload = base::Base64Encode(child_json);
  ASSERT_FALSE(child_payload.empty());

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII(kFingerprintChildDataSwitch, child_payload);
  cmd.AppendSwitchPath(kFingerprintPathSwitch,
                       GetFixturePath("valid_fingerprint.json"));

  auto result = LoadFingerprintFromCommandLine(cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_EQ(FingerprintAccessor::Get()->user_agent, "child-ua");
  EXPECT_EQ(FingerprintAccessor::Get()->timezone, "Etc/GMT-14");
}

TEST_F(FingerprintLoaderTest, CommandLineSpoofingFlagsApplyToChildPayload) {
  auto child_payload = BuildChildFingerprintPayload(
      GetFixturePath("valid_fingerprint.json"));
  ASSERT_TRUE(child_payload.has_value()) << child_payload.error();

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII(kFingerprintChildDataSwitch, *child_payload);
  cmd.AppendSwitch(kEnableCanvasSpoofingSwitch);
  cmd.AppendSwitch(kEnableWebGLSpoofingSwitch);

  auto result = LoadFingerprintFromCommandLine(cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_TRUE(FingerprintAccessor::Get()->canvas_spoofing_enabled);
  EXPECT_TRUE(FingerprintAccessor::Get()->webgl_spoofing_enabled);
}

TEST_F(FingerprintLoaderTest, DisableWebGLSwitchWinsInChildPayload) {
  auto child_payload = BuildChildFingerprintPayload(
      GetFixturePath("valid_fingerprint.json"));
  ASSERT_TRUE(child_payload.has_value()) << child_payload.error();

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII(kFingerprintChildDataSwitch, *child_payload);
  cmd.AppendSwitch(kEnableWebGLSpoofingSwitch);
  cmd.AppendSwitch(kDisableWebGLSpoofingSwitch);

  auto result = LoadFingerprintFromCommandLine(cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_FALSE(FingerprintAccessor::Get()->webgl_spoofing_enabled);
}

TEST_F(FingerprintLoaderTest, ChildPayloadPreservesProxyConfig) {
  auto child_payload = BuildChildFingerprintPayload(
      GetFixturePath("valid_fingerprint.json"));
  ASSERT_TRUE(child_payload.has_value()) << child_payload.error();

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII(kFingerprintChildDataSwitch, *child_payload);

  auto result = LoadFingerprintFromCommandLine(cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  ASSERT_NE(FingerprintAccessor::GetProxy(), nullptr);
  EXPECT_EQ(FingerprintAccessor::GetProxy()->scheme.value_or(""), "http");
  EXPECT_EQ(FingerprintAccessor::GetProxy()->host.value_or(""),
            "proxy.example.com");
  EXPECT_EQ(FingerprintAccessor::GetProxy()->country.value_or(""), "US");
  EXPECT_FALSE(FingerprintAccessor::GetProxy()->username.has_value());
  EXPECT_FALSE(FingerprintAccessor::GetProxy()->password.has_value());
}

TEST_F(FingerprintLoaderTest, LoadEncryptedProxyCredentialsFromProfileCache) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  ProfileEnvelope envelope;
  envelope.schema_version = ProfileEnvelope::kCurrentSchemaVersion;
  envelope.created_at = "2026-04-09T12:00:00Z";
  envelope.request.platform = "macos";
  envelope.request.browser = "chrome";
  envelope.request.country = "US";
  envelope.response = MakeResponseWithProxy("user_abc", "pass_xyz");

  base::FilePath path = temp_dir.GetPath().AppendASCII("fingerprint.json");
  ASSERT_TRUE(base::WriteFile(path, envelope.Serialize()));

  auto result = LoadFingerprint(path);
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_NE(FingerprintAccessor::GetProxy(), nullptr);
  EXPECT_EQ(FingerprintAccessor::GetProxy()->username.value_or(""),
            "user_abc");
  EXPECT_EQ(FingerprintAccessor::GetProxy()->password.value_or(""),
            "pass_xyz");
}

TEST_F(FingerprintLoaderTest, LoadEncryptedProxyCachePreservesProxyScheme) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  ProfileEnvelope envelope;
  envelope.schema_version = ProfileEnvelope::kCurrentSchemaVersion;
  envelope.created_at = "2026-04-09T12:00:00Z";
  envelope.request.platform = "macos";
  envelope.request.browser = "chrome";
  envelope.request.country = "US";
  envelope.response = MakeResponseWithProxy("user_abc", "pass_xyz");
  envelope.response.proxy->scheme = "socks5";

  base::FilePath path = temp_dir.GetPath().AppendASCII("fingerprint.json");
  ASSERT_TRUE(base::WriteFile(path, envelope.Serialize()));

  auto result = LoadFingerprint(path);
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_NE(FingerprintAccessor::GetProxy(), nullptr);
  EXPECT_EQ(FingerprintAccessor::GetProxy()->scheme.value_or(""), "socks5");
  EXPECT_EQ(FingerprintAccessor::GetProxy()->host.value_or(""),
            "proxy.example.com");
  EXPECT_EQ(FingerprintAccessor::GetProxy()->username.value_or(""),
            "user_abc");
  EXPECT_EQ(FingerprintAccessor::GetProxy()->password.value_or(""),
            "pass_xyz");
}

TEST_F(FingerprintLoaderTest,
       BuildChildPayloadFromEncryptedCacheStripsProxyCredentials) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  ProfileEnvelope envelope;
  envelope.schema_version = ProfileEnvelope::kCurrentSchemaVersion;
  envelope.created_at = "2026-04-09T12:00:00Z";
  envelope.request.platform = "macos";
  envelope.request.browser = "chrome";
  envelope.request.country = "US";
  envelope.response = MakeResponseWithProxy("user_abc", "pass_xyz");
  envelope.response.proxy->scheme = "socks5";

  base::FilePath path = temp_dir.GetPath().AppendASCII("fingerprint.json");
  ASSERT_TRUE(base::WriteFile(path, envelope.Serialize()));

  auto child_payload = BuildChildFingerprintPayload(path);
  ASSERT_TRUE(child_payload.has_value()) << child_payload.error();
  std::string child_json;
  ASSERT_TRUE(base::Base64Decode(*child_payload, &child_json));
  EXPECT_EQ(child_json.find("user_abc"), std::string::npos);
  EXPECT_EQ(child_json.find("pass_xyz"), std::string::npos);

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII(kFingerprintChildDataSwitch, *child_payload);

  auto result = LoadFingerprintFromCommandLine(cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_NE(FingerprintAccessor::GetProxy(), nullptr);
  EXPECT_EQ(FingerprintAccessor::GetProxy()->scheme.value_or(""), "socks5");
  EXPECT_FALSE(FingerprintAccessor::GetProxy()->username.has_value());
  EXPECT_FALSE(FingerprintAccessor::GetProxy()->password.has_value());
}

TEST_F(FingerprintLoaderTest, LoadFromCommandLineNoFlag) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  auto result = LoadFingerprintFromCommandLine(cmd);
  // No flag = no load, not an error (vanilla mode)
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

}  // namespace
}  // namespace clawbrowser
