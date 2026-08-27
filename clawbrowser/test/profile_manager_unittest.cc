#include "clawbrowser/cli/profile_manager.h"

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

GenerateResponse MakeMinimalResponse() {
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
  return response;
}

GenerateResponse MakeResponseWithProxy(const std::string& username,
                                      const std::string& password) {
  GenerateResponse response = MakeMinimalResponse();
  response.proxy.emplace();
  response.proxy->host = "proxy.example.com";
  response.proxy->port = 3128;
  response.proxy->country = "US";
  response.proxy->city = "New York";
  response.proxy->username = username;
  response.proxy->password = password;
  return response;
}

class ProfileManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    manager_ = std::make_unique<ProfileManager>(temp_dir_.GetPath());
  }

  void WriteConfigJson(const std::string& api_key,
                       const std::string& base_url = "") {
    base::DictValue config;
    config.Set("api_key", api_key);
    if (!base_url.empty())
      config.Set("api_base_url", base_url);

    std::string json;
    base::JSONWriter::Write(base::Value(std::move(config)), &json);
    base::WriteFile(temp_dir_.GetPath().AppendASCII("config.json"), json);
  }

  void WriteFingerprintProfile(
      const std::string& id,
      const std::string& created_at = "2026-03-23T10:00:00Z") {
    ProfileEnvelope envelope;
    envelope.schema_version = ProfileEnvelope::kCurrentSchemaVersion;
    envelope.created_at = created_at;
    envelope.request.platform = "macos";
    envelope.request.browser = "chrome";
    envelope.request.country = "US";
    envelope.response = MakeMinimalResponse();

    auto save_result = manager_->SaveProfile(id, envelope);
    ASSERT_TRUE(save_result.has_value()) << save_result.error();
  }

  void WriteLegacyFingerprintProfile(const std::string& id) {
    base::FilePath profile_dir =
        temp_dir_.GetPath().AppendASCII("Browser").AppendASCII(id);
    ASSERT_TRUE(base::CreateDirectory(profile_dir));

    std::string envelope = R"({
      "schema_version": 1, "created_at": "2026-03-23T10:00:00Z",
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
          "screen": {"width": 1920, "height": 1080, "avail_width": 1920,
                     "avail_height": 1040, "color_depth": 24, "pixel_ratio": 1.0},
          "hardware": {"concurrency": 8, "memory": 8},
          "webgl": {"vendor": "v", "renderer": "r"},
          "canvas_seed": 1, "audio_seed": 2, "client_rects_seed": 3,
          "timezone": "UTC", "language": ["en"], "fonts": ["Arial"]
        }
      }
    })";
    ASSERT_TRUE(base::WriteFile(profile_dir.AppendASCII("fingerprint.json"),
                                envelope));
  }

  void WriteLegacyNestedFingerprintProfile(const std::string& id) {
    base::FilePath profile_dir =
        temp_dir_.GetPath().AppendASCII("Browser").Append(
            base::FilePath::FromUTF8Unsafe(id));
    ASSERT_TRUE(base::CreateDirectory(profile_dir));

    std::string envelope = R"({
      "schema_version": 1, "created_at": "2026-03-23T10:00:00Z",
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
          "screen": {"width": 1920, "height": 1080, "avail_width": 1920,
                     "avail_height": 1040, "color_depth": 24, "pixel_ratio": 1.0},
          "hardware": {"concurrency": 8, "memory": 8},
          "webgl": {"vendor": "v", "renderer": "r"},
          "canvas_seed": 1, "audio_seed": 2, "client_rects_seed": 3,
          "timezone": "UTC", "language": ["en"], "fonts": ["Arial"]
        }
      }
    })";
    ASSERT_TRUE(base::WriteFile(profile_dir.AppendASCII("fingerprint.json"),
                                envelope));
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ProfileManager> manager_;
};

TEST_F(ProfileManagerTest, ApiKeyFromConfigJson) {
  WriteConfigJson("key_from_config");
  auto key = manager_->ResolveApiKey();
  ASSERT_TRUE(key.has_value());
  EXPECT_EQ(*key, "key_from_config");
}

TEST_F(ProfileManagerTest, ApiKeyFromEnvVar) {
  auto env = base::Environment::Create();
  env->SetVar("CLAWBROWSER_API_KEY", "key_from_env");
  // Env var takes precedence over config.json
  WriteConfigJson("key_from_config");
  auto key = manager_->ResolveApiKey();
  ASSERT_TRUE(key.has_value());
  EXPECT_EQ(*key, "key_from_env");
  env->UnSetVar("CLAWBROWSER_API_KEY");
}

TEST_F(ProfileManagerTest, ApiKeyMissing) {
  auto key = manager_->ResolveApiKey();
  ASSERT_FALSE(key.has_value());
}

TEST_F(ProfileManagerTest, BaseUrlFromConfigJson) {
  WriteConfigJson("key_from_config", "http://127.0.0.1:8787");
  auto base_url = manager_->ResolveBaseUrl();
  ASSERT_TRUE(base_url.has_value());
  EXPECT_EQ(*base_url, "http://127.0.0.1:8787");
}

TEST_F(ProfileManagerTest, BaseUrlFromEnvVar) {
  auto env = base::Environment::Create();
  env->SetVar("CLAWBROWSER_API_BASE_URL", "http://127.0.0.1:9999");
  WriteConfigJson("key_from_config", "http://127.0.0.1:8787");
  auto base_url = manager_->ResolveBaseUrl();
  ASSERT_TRUE(base_url.has_value());
  EXPECT_EQ(*base_url, "http://127.0.0.1:9999");
  env->UnSetVar("CLAWBROWSER_API_BASE_URL");
}

TEST_F(ProfileManagerTest, BaseUrlUsesBuildDefaultOnlyWhenConfigured) {
  auto build_default = GetBuildDefaultApiBaseUrl();
  auto base_url = manager_->ResolveBaseUrl();

  ASSERT_TRUE(build_default.has_value());
  ASSERT_TRUE(base_url.has_value());
  EXPECT_EQ(*base_url, *build_default);
}

TEST_F(ProfileManagerTest, SaveApiKeyToConfigFallsBackToBuildDefaultBaseUrl) {
  auto save_result = manager_->SaveApiKey("saved_key");
  ASSERT_TRUE(save_result.has_value()) << save_result.error();

  auto key = manager_->ResolveApiKey();
  ASSERT_TRUE(key.has_value());
  EXPECT_EQ(*key, "saved_key");
  auto build_default = GetBuildDefaultApiBaseUrl();
  ASSERT_TRUE(build_default.has_value());
  auto base_url = manager_->ResolveBaseUrl();
  ASSERT_TRUE(base_url.has_value());
  EXPECT_EQ(*base_url, *build_default);
}

TEST_F(ProfileManagerTest, SaveApiKeyToConfigPreservesBaseUrl) {
  WriteConfigJson("", "http://127.0.0.1:8787");

  auto save_result = manager_->SaveApiKey("saved_key");
  ASSERT_TRUE(save_result.has_value()) << save_result.error();

  auto key = manager_->ResolveApiKey();
  ASSERT_TRUE(key.has_value());
  EXPECT_EQ(*key, "saved_key");
  auto base_url = manager_->ResolveBaseUrl();
  ASSERT_TRUE(base_url.has_value());
  EXPECT_EQ(*base_url, "http://127.0.0.1:8787");
}

TEST_F(ProfileManagerTest, ListProfiles) {
  WriteFingerprintProfile("abc123_profile");
  WriteFingerprintProfile("def456_profile");

  auto profiles = manager_->ListProfiles();
  EXPECT_EQ(profiles.size(), 2u);
  // Should contain both IDs (order may vary)
  bool found_abc = false, found_def = false;
  for (const auto& p : profiles) {
    if (p.id == "abc123_profile") found_abc = true;
    if (p.id == "def456_profile") found_def = true;
  }
  EXPECT_TRUE(found_abc);
  EXPECT_TRUE(found_def);
}

TEST_F(ProfileManagerTest, ListProfilesIncludesIdsWithoutPrefix) {
  WriteLegacyFingerprintProfile("custom_profile");

  auto profiles = manager_->ListProfiles();
  ASSERT_EQ(profiles.size(), 1u);
  EXPECT_EQ(profiles[0].id, "custom_profile");
}

TEST_F(ProfileManagerTest, ListProfilesPreservesLegacyIdsThatLookEncoded) {
  WriteLegacyFingerprintProfile("id_6162");

  auto profiles = manager_->ListProfiles();
  ASSERT_EQ(profiles.size(), 1u);
  EXPECT_EQ(profiles[0].id, "id_6162");
}

TEST_F(ProfileManagerTest, ListProfilesIncludesPathLikeIds) {
  WriteFingerprintProfile("group/profile");

  auto profiles = manager_->ListProfiles();
  ASSERT_EQ(profiles.size(), 1u);
  EXPECT_EQ(profiles[0].id, "group/profile");
}

TEST_F(ProfileManagerTest, ListProfilesIncludesLegacyNestedPathLikeIds) {
  WriteLegacyNestedFingerprintProfile("group/profile");

  auto profiles = manager_->ListProfiles();
  ASSERT_EQ(profiles.size(), 1u);
  EXPECT_EQ(profiles[0].id, "group/profile");
}

TEST_F(ProfileManagerTest, ListProfilesEmpty) {
  auto profiles = manager_->ListProfiles();
  EXPECT_TRUE(profiles.empty());
}

TEST_F(ProfileManagerTest, FindBestCachedProfileIdUsesNewestCreatedAt) {
  WriteFingerprintProfile("old_profile", "2026-03-22T10:00:00Z");
  WriteFingerprintProfile("new_profile", "2026-03-24T10:00:00Z");

  auto profile_id = manager_->FindBestCachedProfileId();

  ASSERT_TRUE(profile_id.has_value());
  EXPECT_EQ(*profile_id, "new_profile");
}

TEST_F(ProfileManagerTest, FindBestCachedProfileIdEmptyWithoutProfiles) {
  EXPECT_FALSE(manager_->FindBestCachedProfileId().has_value());
}

TEST_F(ProfileManagerTest, GetFingerprintPath) {
  WriteFingerprintProfile("abc123_profile");

  auto path = manager_->GetFingerprintPath("abc123_profile");
  EXPECT_TRUE(base::PathExists(path));
}

TEST_F(ProfileManagerTest, GetUserDataDir) {
  auto dir = manager_->GetUserDataDir("abc123_profile");
  EXPECT_FALSE(dir.empty());
}

TEST_F(ProfileManagerTest, GetVanillaUserDataDir) {
  auto dir = manager_->GetVanillaUserDataDir();
  // FilePath::StringType is std::wstring on Windows and std::string on POSIX,
  // so the needle and the npos constant both have to come from StringType.
  EXPECT_NE(dir.value().find(FILE_PATH_LITERAL("Default")),
            base::FilePath::StringType::npos);
}

TEST_F(ProfileManagerTest, SaveAndReadEnvelope) {
  ProfileEnvelope envelope;
  envelope.schema_version = ProfileEnvelope::kCurrentSchemaVersion;
  envelope.created_at = "2026-03-23T10:00:00Z";
  envelope.request.platform = "macos";
  envelope.request.browser = "chrome";
  envelope.request.country = "US";
  envelope.response = MakeMinimalResponse();

  auto save_result = manager_->SaveProfile("saved_profile", envelope);
  ASSERT_TRUE(save_result.has_value()) << save_result.error();

  auto read_result = manager_->ReadProfile("saved_profile");
  ASSERT_TRUE(read_result.has_value()) << read_result.error();
  EXPECT_EQ(read_result->schema_version, ProfileEnvelope::kCurrentSchemaVersion);
  EXPECT_EQ(read_result->profile_id.value_or(""), "saved_profile");
  EXPECT_EQ(read_result->response.fingerprint.user_agent, "test-ua");
}

TEST_F(ProfileManagerTest, SaveAndReadEnvelopeWithPathLikeId) {
  ProfileEnvelope envelope;
  envelope.schema_version = ProfileEnvelope::kCurrentSchemaVersion;
  envelope.created_at = "2026-03-23T10:00:00Z";
  envelope.request.platform = "macos";
  envelope.request.browser = "chrome";
  envelope.request.country = "US";
  envelope.response = MakeMinimalResponse();

  auto save_result = manager_->SaveProfile("group/profile", envelope);
  ASSERT_TRUE(save_result.has_value()) << save_result.error();

  auto read_result = manager_->ReadProfile("group/profile");
  ASSERT_TRUE(read_result.has_value()) << read_result.error();
  EXPECT_EQ(read_result->profile_id.value_or(""), "group/profile");
  EXPECT_EQ(read_result->response.fingerprint.user_agent, "test-ua");
}

TEST_F(ProfileManagerTest, SaveAndReadMultipleProfilesAreIsolated) {
  ProfileEnvelope first;
  first.schema_version = ProfileEnvelope::kCurrentSchemaVersion;
  first.created_at = "2026-03-23T10:00:00Z";
  first.request.platform = "macos";
  first.request.browser = "chrome";
  first.request.country = "US";
  first.response = MakeMinimalResponse();
  first.response.fingerprint.user_agent = "first-profile-ua";

  ProfileEnvelope second = first;
  second.response.fingerprint.user_agent = "second-profile-ua";

  auto first_save = manager_->SaveProfile("work_profile", first);
  ASSERT_TRUE(first_save.has_value()) << first_save.error();
  auto second_save = manager_->SaveProfile("personal_profile", second);
  ASSERT_TRUE(second_save.has_value()) << second_save.error();
  EXPECT_NE(manager_->GetUserDataDir("work_profile"),
            manager_->GetUserDataDir("personal_profile"));

  auto first_read = manager_->ReadProfile("work_profile");
  ASSERT_TRUE(first_read.has_value()) << first_read.error();
  auto second_read = manager_->ReadProfile("personal_profile");
  ASSERT_TRUE(second_read.has_value()) << second_read.error();

  EXPECT_EQ(first_read->profile_id.value_or(""), "work_profile");
  EXPECT_EQ(second_read->profile_id.value_or(""), "personal_profile");
  EXPECT_EQ(first_read->response.fingerprint.user_agent, "first-profile-ua");
  EXPECT_EQ(second_read->response.fingerprint.user_agent, "second-profile-ua");
}

TEST_F(ProfileManagerTest, ReadProfileSupportsLegacyNestedPathLikeIds) {
  WriteLegacyNestedFingerprintProfile("group/profile");

  EXPECT_TRUE(manager_->HasCachedProfile("group/profile"));
  auto read_result = manager_->ReadProfile("group/profile");
  ASSERT_TRUE(read_result.has_value()) << read_result.error();
  EXPECT_EQ(read_result->profile_id.value_or(""), "group/profile");
  EXPECT_EQ(read_result->response.fingerprint.user_agent, "test");
}

TEST_F(ProfileManagerTest, SaveAndReadEnvelopeEncryptsProxyCredentials) {
  ProfileEnvelope envelope;
  envelope.schema_version = ProfileEnvelope::kCurrentSchemaVersion;
  envelope.created_at = "2026-03-23T10:00:00Z";
  envelope.request.platform = "macos";
  envelope.request.browser = "chrome";
  envelope.request.country = "US";
  envelope.response = MakeResponseWithProxy("user_abc", "pass_xyz");

  auto save_result = manager_->SaveProfile("secret_profile", envelope);
  ASSERT_TRUE(save_result.has_value()) << save_result.error();

  std::string raw_json;
  ASSERT_TRUE(base::ReadFileToString(
      manager_->GetFingerprintPath("secret_profile"), &raw_json));
  EXPECT_EQ(raw_json.find("user_abc"), std::string::npos);
  EXPECT_EQ(raw_json.find("pass_xyz"), std::string::npos);
  EXPECT_NE(raw_json.find("encrypted_proxy_credentials"), std::string::npos);

  auto read_result = manager_->ReadProfile("secret_profile");
  ASSERT_TRUE(read_result.has_value()) << read_result.error();
  ASSERT_TRUE(read_result->response.proxy.has_value());
  EXPECT_EQ(read_result->response.proxy->username.value_or(""), "user_abc");
  EXPECT_EQ(read_result->response.proxy->password.value_or(""), "pass_xyz");
}

// POSIX-only: this makes a directory unwritable via chmod. Windows has no
// equivalent that is reliable in a test -- read-only ACLs are bypassed for
// elevated processes, which would make the expectation flaky rather than
// portable.
#if BUILDFLAG(IS_POSIX)
TEST_F(ProfileManagerTest, SaveProfileDirectoryNotWritable) {
  // Create a read-only directory to simulate write failure
  base::FilePath readonly_dir = temp_dir_.GetPath().AppendASCII("readonly");
  ASSERT_TRUE(base::CreateDirectory(readonly_dir));
  ASSERT_TRUE(base::SetPosixFilePermissions(readonly_dir, 0555));

  ProfileManager readonly_manager(readonly_dir);
  ProfileEnvelope envelope;
  envelope.schema_version = 1;
  envelope.created_at = "2026-03-23T10:00:00Z";
  envelope.request.platform = "macos";
  envelope.request.browser = "chrome";
  envelope.request.country = "US";
  envelope.response = MakeMinimalResponse();

  auto result = readonly_manager.SaveProfile("fail_profile", envelope);
  EXPECT_FALSE(result.has_value());
  EXPECT_FALSE(result.error().empty());

  // Cleanup: restore permissions so temp dir cleanup works
  base::SetPosixFilePermissions(readonly_dir, 0755);
}
#endif  // BUILDFLAG(IS_POSIX)

}  // namespace
}  // namespace clawbrowser
