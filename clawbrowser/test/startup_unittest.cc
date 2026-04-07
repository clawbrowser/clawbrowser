#include "clawbrowser/startup.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "clawbrowser/cli/args.h"
#include "clawbrowser/fingerprint_accessor.h"
#include "clawbrowser/fingerprint_loader.h"
#include "clawbrowser/logging.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_version.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

class StartupTest : public testing::Test {
 protected:
  static std::string HttpStatusLine(net::HttpStatusCode status) {
    switch (status) {
      case net::HTTP_UNAUTHORIZED:
        return "401 Unauthorized";
      default:
        return base::StringPrintf("%d", static_cast<int>(status));
    }
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    home_override_ =
        std::make_unique<base::ScopedPathOverride>(base::DIR_HOME,
                                                   temp_dir_.GetPath());
    // Override HOME so ProfileManager uses our temp dir
    env_ = base::Environment::Create();
    env_->SetVar("HOME", temp_dir_.GetPath().AsUTF8Unsafe());
    // Create config dir structure
    base::FilePath config_dir =
        temp_dir_.GetPath().AppendASCII(".config/clawbrowser");
    base::CreateDirectory(config_dir);
  }

  void TearDown() override {
    FingerprintAccessor::Reset();
    env_->UnSetVar("HOME");
    env_->UnSetVar("CLAWBROWSER_API_KEY");
  }

  void WriteConfigJson(const std::string& api_key) {
    base::FilePath config_path = temp_dir_.GetPath()
        .AppendASCII(".config/clawbrowser/config.json");
    base::WriteFile(config_path,
                    "{\"api_key\": \"" + api_key + "\"}");
  }

  void WriteCachedProfile(const std::string& id) {
    base::FilePath profile_dir = temp_dir_.GetPath()
        .AppendASCII(".config/clawbrowser/Browser").AppendASCII(id);
    base::CreateDirectory(profile_dir);
    // Read from test fixture
    base::FilePath fixture;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &fixture);
    fixture = fixture.AppendASCII("clawbrowser")
                  .AppendASCII("test")
                  .AppendASCII("fixtures")
                  .AppendASCII("valid_fingerprint.json");
    std::string json;
    base::ReadFileToString(fixture, &json);
    base::WriteFile(profile_dir.AppendASCII("fingerprint.json"), json);
  }

  void AddGenerateErrorResponse(const std::string& body,
                                net::HttpStatusCode status) {
    auto head = network::mojom::URLResponseHead::New();
    head->headers =
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1),
                                          HttpStatusLine(status))
            .Build();
    url_loader_factory_.AddResponse(
        GURL("https://api.clawbrowser.ai/v1/fingerprints/generate"),
        std::move(head), body, network::URLLoaderCompletionStatus(net::OK));
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> home_override_;
  std::unique_ptr<base::Environment> env_;
  network::TestURLLoaderFactory url_loader_factory_;
};

TEST_F(StartupTest, VanillaMode) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->should_exit);
  // Should set --user-data-dir to Default
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_NE(cmd.GetSwitchValueASCII("user-data-dir").find("Default"),
            std::string::npos);
  // Accessor should be null (no fingerprint)
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(StartupTest, ListProfiles) {
  WriteCachedProfile("fp_test1");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitch("list");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->should_exit);
  EXPECT_EQ(result->exit_code, 0);
}

TEST_F(StartupTest, FingerprintWithCachedProfile) {
  WriteCachedProfile("fp_cached");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "fp_cached");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  // Fingerprint should be loaded
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  // Command line flags should be set
  EXPECT_TRUE(cmd.HasSwitch(kFingerprintPathSwitch));
  EXPECT_FALSE(cmd.HasSwitch("clawbrowser-fp-data"));
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_TRUE(cmd.HasSwitch("proxy-server"));
  EXPECT_TRUE(cmd.HasSwitch("lang"));
  EXPECT_TRUE(cmd.HasSwitch("accept-lang"));
  EXPECT_TRUE(cmd.HasSwitch("user-agent"));
  EXPECT_EQ(cmd.GetSwitchValueASCII("user-agent"),
            FingerprintAccessor::Get()->user_agent);
  EXPECT_EQ(cmd.GetSwitchValueASCII("clawbrowser-ua-major-version"), "120");
  EXPECT_EQ(cmd.GetSwitchValueASCII("clawbrowser-ua-full-version"),
            "120.0.0.0");
  EXPECT_EQ(cmd.GetSwitchValueASCII("clawbrowser-ua-platform"), "macOS");
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(cmd.HasSwitch(kFingerprintChildDataSwitch));
#else
  EXPECT_FALSE(cmd.HasSwitch(kFingerprintChildDataSwitch));
#endif
}

TEST_F(StartupTest, ConfigureEarlyStartupSetsFingerprintUserDataDir) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "fp_early_profile");

  auto result = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_NE(cmd.GetSwitchValueASCII("user-data-dir").find("fp_early_profile"),
            std::string::npos);
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(StartupTest, FingerprintNoApiKey) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "fp_new");
  // No cached profile, no API key -> should fail
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->should_exit);
  EXPECT_EQ(result->exit_code, 1);
}

TEST_F(StartupTest, FingerprintApiCallSuccess) {
  WriteConfigJson("test_key");
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");

  // Mock API response
  std::string api_response = R"({
    "fingerprint": {
      "user_agent": "test-ua", "platform": "test",
      "screen": {"width": 1920, "height": 1080, "avail_width": 1920,
                 "avail_height": 1040, "color_depth": 24, "pixel_ratio": 1.0},
      "hardware": {"concurrency": 8, "memory": 8},
      "webgl": {"vendor": "test", "renderer": "test"},
      "canvas_seed": 123, "audio_seed": 456, "client_rects_seed": 789,
      "timezone": "UTC", "language": ["en"], "fonts": ["Arial"]
    }
  })";
  url_loader_factory_.AddResponse(
      "https://api.clawbrowser.ai/v1/fingerprints/generate",
      api_response);

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "fp_new_profile");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_EQ(FingerprintAccessor::Get()->user_agent, "test-ua");
}

TEST_F(StartupTest, EmptyFingerprintIdExitsWithError) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->should_exit);
  EXPECT_EQ(result->exit_code, 1);
}

TEST_F(StartupTest, InvalidFingerprintIdPrefixExitsWithError) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "not_a_valid_id");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->should_exit);
  EXPECT_EQ(result->exit_code, 1);
}

TEST_F(StartupTest, FingerprintApiCall401) {
  env_->SetVar("CLAWBROWSER_API_KEY", "bad_key");

  AddGenerateErrorResponse(
      R"({"code": "invalid_api_key", "message": "invalid API key"})",
      net::HTTP_UNAUTHORIZED);

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "fp_bad_key");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->should_exit);
  EXPECT_EQ(result->exit_code, 1);
}

TEST_F(StartupTest, RegenerateReplaysStoredParams) {
  WriteCachedProfile("fp_regen");
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");

  // Mock API — we just need it to succeed
  url_loader_factory_.AddResponse(
      "https://api.clawbrowser.ai/v1/fingerprints/generate",
      R"({
        "fingerprint": {
          "user_agent": "new-ua", "platform": "test",
          "screen": {"width": 1920, "height": 1080, "avail_width": 1920,
                     "avail_height": 1040, "color_depth": 24, "pixel_ratio": 1.0},
          "hardware": {"concurrency": 8, "memory": 8},
          "webgl": {"vendor": "v", "renderer": "r"},
          "canvas_seed": 1, "audio_seed": 2, "client_rects_seed": 3,
          "timezone": "UTC", "language": ["en"], "fonts": ["Arial"]
        }
      })");

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "fp_regen");
  cmd.AppendSwitch("regenerate");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  // New fingerprint should be loaded
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_EQ(FingerprintAccessor::Get()->user_agent, "new-ua");
}

TEST_F(StartupTest, VerboseLogging) {
  WriteCachedProfile("fp_verbose");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "fp_verbose");
  cmd.AppendSwitch("verbose");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(IsVerbose());
}

TEST_F(StartupTest, JsonErrorOutput) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "fp_missing");
  cmd.AppendSwitchASCII("output", "json");
  // No cached profile, no API key -> error

  // Capture stdout to verify JSON error format
  testing::internal::CaptureStdout();
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  std::string stdout_output = testing::internal::GetCapturedStdout();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->exit_code, 1);

  // Verify JSON output contains error code and message
  auto parsed = base::JSONReader::Read(stdout_output, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed.has_value()) << "stdout not valid JSON: " << stdout_output;
  ASSERT_TRUE(parsed->is_dict());
  const std::string* error_code = parsed->GetDict().FindString("error");
  ASSERT_NE(error_code, nullptr);
  EXPECT_EQ(*error_code, "no_api_key");
  const std::string* message = parsed->GetDict().FindString("message");
  ASSERT_NE(message, nullptr);
  EXPECT_FALSE(message->empty());
}

TEST_F(StartupTest, MultipleFingerprintFlagsLastWins) {
  // When --fingerprint is specified multiple times, last value wins
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "fp_first");
  cmd.AppendSwitchASCII("fingerprint", "fp_second");

  ClawArgs args = ClawArgs::Parse(cmd);
  // base::CommandLine last-wins semantics for duplicate switches
  EXPECT_EQ(args.fingerprint_id(), "fp_second");
}

}  // namespace
}  // namespace clawbrowser
