#include "clawbrowser/cli/profile_manager.h"

#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/values.h"
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

  void WriteFingerprintProfile(const std::string& id) {
    base::FilePath profile_dir =
        temp_dir_.GetPath().AppendASCII("Browser").AppendASCII(id);
    base::CreateDirectory(profile_dir);

    std::string envelope = R"({
      "schema_version": 1, "created_at": "2026-03-23T10:00:00Z",
      "request": {"platform": "macos", "browser": "chrome", "country": "US"},
      "response": {
        "fingerprint": {
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
    base::WriteFile(profile_dir.AppendASCII("fingerprint.json"), envelope);
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
  EXPECT_EQ(manager_->ResolveBaseUrl(), "http://127.0.0.1:8787");
}

TEST_F(ProfileManagerTest, BaseUrlFromEnvVar) {
  auto env = base::Environment::Create();
  env->SetVar("CLAWBROWSER_API_BASE_URL", "http://127.0.0.1:9999");
  WriteConfigJson("key_from_config", "http://127.0.0.1:8787");
  EXPECT_EQ(manager_->ResolveBaseUrl(), "http://127.0.0.1:9999");
  env->UnSetVar("CLAWBROWSER_API_BASE_URL");
}

TEST_F(ProfileManagerTest, ListProfiles) {
  WriteFingerprintProfile("fp_abc123");
  WriteFingerprintProfile("fp_def456");

  auto profiles = manager_->ListProfiles();
  EXPECT_EQ(profiles.size(), 2u);
  // Should contain both IDs (order may vary)
  bool found_abc = false, found_def = false;
  for (const auto& p : profiles) {
    if (p.id == "fp_abc123") found_abc = true;
    if (p.id == "fp_def456") found_def = true;
  }
  EXPECT_TRUE(found_abc);
  EXPECT_TRUE(found_def);
}

TEST_F(ProfileManagerTest, ListProfilesEmpty) {
  auto profiles = manager_->ListProfiles();
  EXPECT_TRUE(profiles.empty());
}

TEST_F(ProfileManagerTest, GetFingerprintPath) {
  WriteFingerprintProfile("fp_abc123");

  auto path = manager_->GetFingerprintPath("fp_abc123");
  EXPECT_TRUE(base::PathExists(path));
  EXPECT_NE(path.value().find("fp_abc123"), std::string::npos);
}

TEST_F(ProfileManagerTest, GetUserDataDir) {
  auto dir = manager_->GetUserDataDir("fp_abc123");
  EXPECT_NE(dir.value().find("fp_abc123"), std::string::npos);
}

TEST_F(ProfileManagerTest, GetVanillaUserDataDir) {
  auto dir = manager_->GetVanillaUserDataDir();
  EXPECT_NE(dir.value().find("Default"), std::string::npos);
}

TEST_F(ProfileManagerTest, SaveAndReadEnvelope) {
  ProfileEnvelope envelope;
  envelope.schema_version = 1;
  envelope.created_at = "2026-03-23T10:00:00Z";
  envelope.request.platform = "macos";
  envelope.request.browser = "chrome";
  envelope.request.country = "US";
  envelope.response = MakeMinimalResponse();

  auto save_result = manager_->SaveProfile("fp_test", envelope);
  ASSERT_TRUE(save_result.has_value()) << save_result.error();

  auto read_result = manager_->ReadProfile("fp_test");
  ASSERT_TRUE(read_result.has_value()) << read_result.error();
  EXPECT_EQ(read_result->schema_version, 1);
  EXPECT_EQ(read_result->response.fingerprint.user_agent, "test-ua");
}

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

  auto result = readonly_manager.SaveProfile("fp_fail", envelope);
  EXPECT_FALSE(result.has_value());
  EXPECT_FALSE(result.error().empty());

  // Cleanup: restore permissions so temp dir cleanup works
  base::SetPosixFilePermissions(readonly_dir, 0755);
}

}  // namespace
}  // namespace clawbrowser
