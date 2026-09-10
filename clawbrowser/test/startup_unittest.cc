#include "clawbrowser/startup.h"

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "clawbrowser/cli/args.h"
#include "clawbrowser/cli/profile_manager.h"
#include "clawbrowser/fingerprint_accessor.h"
#include "clawbrowser/fingerprint_loader.h"
#include "clawbrowser/logging.h"
#include "clawbrowser/profile_envelope.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_version.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#endif

namespace clawbrowser {
namespace {

constexpr char kConfiguredApiBaseUrl[] = "http://127.0.0.1:8787";

class StartupTest : public testing::Test {
 protected:
  static std::string HttpStatusLine(net::HttpStatusCode status) {
    switch (status) {
      case net::HTTP_UNAUTHORIZED:
        return "401 Unauthorized";
      case net::HTTP_FORBIDDEN:
        return "403 Forbidden";
      case net::HTTP_INTERNAL_SERVER_ERROR:
        return "500 Internal Server Error";
      default:
        return base::StringPrintf("%d", static_cast<int>(status));
    }
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
#if BUILDFLAG(IS_WIN)
    local_app_data_override_ =
        std::make_unique<base::ScopedPathOverride>(base::DIR_LOCAL_APP_DATA,
                                                   temp_dir_.GetPath());
#else
    home_override_ =
        std::make_unique<base::ScopedPathOverride>(base::DIR_HOME,
                                                   temp_dir_.GetPath());
#endif
    // Keep environment-based path fallbacks inside the test temp dir.
    env_ = base::Environment::Create();
    env_->SetVar("HOME", temp_dir_.GetPath().AsUTF8Unsafe());
    // Create config dir structure
    base::CreateDirectory(GetConfigDir());
  }

  void TearDown() override {
    FingerprintAccessor::Reset();
    env_->UnSetVar("HOME");
    env_->UnSetVar("CLAWBROWSER_API_KEY");
    env_->UnSetVar("CLAWBROWSER_API_BASE_URL");
    env_->UnSetVar("CLAWBROWSER_DEFAULT_FINGERPRINT_ID");
    env_->UnSetVar("CLAWBROWSER_DEV_PROXY_URL");
  }

  void WriteConfigJson(const std::string& api_key,
                       const std::string& base_url = "") {
    base::FilePath config_path = GetConfigDir().AppendASCII("config.json");
    std::string json = "{\"api_key\": \"" + api_key + "\"";
    if (!base_url.empty()) {
      json += ", \"api_base_url\": \"" + base_url + "\"";
    }
    json += "}";
    base::WriteFile(config_path, json);
  }

  base::FilePath GetConfigDir() const {
#if BUILDFLAG(IS_WIN)
    return temp_dir_.GetPath().AppendASCII("Clawbrowser");
#else
    return temp_dir_.GetPath().AppendASCII(".config").AppendASCII("clawbrowser");
#endif
  }

  ProfileManager CreateProfileManager() const {
    return ProfileManager(GetConfigDir());
  }

  std::string NormalizePathForComparison(const base::FilePath& path) const {
    base::FilePath normalized_path;
    std::string path_string =
        base::NormalizeFilePath(path, &normalized_path)
            ? normalized_path.AsUTF8Unsafe()
            : path.AsUTF8Unsafe();
#if BUILDFLAG(IS_MAC)
    if (path_string.rfind("/var/", 0) == 0 ||
        path_string.rfind("/tmp/", 0) == 0) {
      path_string = "/private" + path_string;
    }
#endif
    return path_string;
  }

  std::string UserDataDirSwitch(const base::CommandLine& cmd) const {
    return NormalizePathForComparison(base::FilePath::FromUTF8Unsafe(
        cmd.GetSwitchValueASCII("user-data-dir")));
  }

  void WriteCachedProfile(const std::string& id) {
    base::FilePath fixture;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &fixture);
    fixture = fixture.AppendASCII("clawbrowser")
                  .AppendASCII("test")
                  .AppendASCII("fixtures")
                  .AppendASCII("valid_fingerprint.json");
    std::string json;
    ASSERT_TRUE(base::ReadFileToString(fixture, &json));
    auto envelope = ProfileEnvelope::Parse(json);
    ASSERT_TRUE(envelope.has_value()) << envelope.error();
    auto save_result = CreateProfileManager().SaveProfile(id, *envelope);
    ASSERT_TRUE(save_result.has_value()) << save_result.error();
  }

  void WriteLegacyRawCachedProfile(const std::string& id) {
    base::FilePath profile_dir =
        GetConfigDir().AppendASCII("Browser").AppendASCII(id);
    ASSERT_TRUE(base::CreateDirectory(profile_dir));

    base::FilePath fixture;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &fixture);
    fixture = fixture.AppendASCII("clawbrowser")
                  .AppendASCII("test")
                  .AppendASCII("fixtures")
                  .AppendASCII("valid_fingerprint.json");
    std::string json;
    ASSERT_TRUE(base::ReadFileToString(fixture, &json));
    ASSERT_TRUE(base::WriteFile(profile_dir.AppendASCII("fingerprint.json"),
                                json));
  }

  void WriteLegacyPrivacyPolicyCachedProfile(const std::string& id) {
    WriteCachedProfile(id);
    ProfileEnvelope envelope = ReadSavedProfile(id);
    envelope.response.fingerprint.surface_policy.canvas.mode = "native";
    envelope.response.fingerprint.surface_policy.webgl.mode = "native";
    auto save_result = CreateProfileManager().SaveProfile(id, envelope);
    ASSERT_TRUE(save_result.has_value()) << save_result.error();
  }

  void WriteCachedProfileWithProxy(const std::string& id,
                                   const std::string& username,
                                   const std::string& password) {
    ProfileEnvelope envelope;
    envelope.schema_version = ProfileEnvelope::kCurrentSchemaVersion;
    envelope.created_at = "2026-04-09T12:00:00Z";
    envelope.request.platform = "macos";
    envelope.request.browser = "chrome";
    envelope.request.country = "US";
    envelope.response.fingerprint.user_agent = "stale-ua";
    envelope.response.fingerprint.platform = "MacIntel";
    envelope.response.fingerprint.screen.width = 1920;
    envelope.response.fingerprint.screen.height = 1080;
    envelope.response.fingerprint.screen.avail_width = 1920;
    envelope.response.fingerprint.screen.avail_height = 1040;
    envelope.response.fingerprint.screen.color_depth = 24;
    envelope.response.fingerprint.screen.pixel_ratio = 1.0;
    envelope.response.fingerprint.hardware.concurrency = 8;
    envelope.response.fingerprint.hardware.memory = 8;
    envelope.response.fingerprint.webgl.vendor = "vendor";
    envelope.response.fingerprint.webgl.renderer = "renderer";
    envelope.response.fingerprint.canvas_seed = 1;
    envelope.response.fingerprint.audio_seed = 2;
    envelope.response.fingerprint.client_rects_seed = 3;
    envelope.response.fingerprint.timezone = "UTC";
    envelope.response.fingerprint.language = {"en-US"};
    envelope.response.fingerprint.fonts = {"Arial"};
    envelope.response.proxy.emplace();
    envelope.response.proxy->host = "proxy.example.com";
    envelope.response.proxy->port = 3128;
    envelope.response.proxy->country = "US";
    envelope.response.proxy->city = "New York";
    envelope.response.proxy->username = username;
    envelope.response.proxy->password = password;

    auto save_result = CreateProfileManager().SaveProfile(id, envelope);
    ASSERT_TRUE(save_result.has_value()) << save_result.error();
  }

  void WriteCachedProfileWithRequest(const std::string& id,
                                     const GenerateRequest& request) {
    base::FilePath fixture;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &fixture);
    fixture = fixture.AppendASCII("clawbrowser")
                  .AppendASCII("test")
                  .AppendASCII("fixtures")
                  .AppendASCII("valid_fingerprint.json");
    std::string json;
    ASSERT_TRUE(base::ReadFileToString(fixture, &json));
    auto envelope = ProfileEnvelope::Parse(json);
    ASSERT_TRUE(envelope.has_value()) << envelope.error();
    envelope->request = request;
    auto save_result = CreateProfileManager().SaveProfile(id, *envelope);
    ASSERT_TRUE(save_result.has_value()) << save_result.error();
  }

  ProfileEnvelope ReadSavedProfile(const std::string& id) {
    auto read_result = CreateProfileManager().ReadProfile(id);
    EXPECT_TRUE(read_result.has_value()) << read_result.error();
    return *read_result;
  }

  void AddGenerateErrorResponse(const std::string& body,
                                net::HttpStatusCode status) {
    auto head = network::mojom::URLResponseHead::New();
    head->headers =
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1),
                                          HttpStatusLine(status))
            .Build();
    url_loader_factory_.AddResponse(
        GURL(std::string(kConfiguredApiBaseUrl) + "/v1/fingerprints/generate"),
        std::move(head), body, network::URLLoaderCompletionStatus(net::OK));
  }

  std::string GenerateSuccessResponseJson(
      const std::string& user_agent,
      const std::string& proxy_json = "") const {
    std::string proxy_field;
    if (!proxy_json.empty()) {
      proxy_field = ",\"proxy\":" + proxy_json;
    }
    return base::StringPrintf(R"json({
      "fingerprint": {
        "browser_family": "chrome",
        "browser_version": "120.0.0.0",
        "engine": "blink",
        "os": "macos",
        "os_version": "10.15.7",
        "architecture": "arm64",
        "device_class": "desktop",
        "user_agent": "%s",
        "platform": "MacIntel",
        "user_agent_data": {
          "brands": [
            {"brand": "Chromium", "version": "120"},
            {"brand": "Google Chrome", "version": "120"},
            {"brand": "Not/A)Brand", "version": "99"}
          ],
          "fullVersionList": [
            {"brand": "Chromium", "version": "120.0.0.0"},
            {"brand": "Google Chrome", "version": "120.0.0.0"},
            {"brand": "Not/A)Brand", "version": "99.0.0.0"}
          ],
          "platform": "macOS",
          "platformVersion": "10.15.7",
          "architecture": "arm",
          "bitness": "64",
          "mobile": false,
          "model": "",
          "uaFullVersion": "120.0.0.0"
        },
        "screen": {"width": 1920, "height": 1080, "avail_width": 1920,
                   "avail_height": 1040, "color_depth": 24, "pixel_ratio": 1.0},
        "hardware": {"concurrency": 8, "memory": 8},
        "webgl": {"vendor": "v", "renderer": "r"},
        "canvas_seed": 1, "audio_seed": 2, "client_rects_seed": 3,
        "timezone": "UTC",
        "language": ["en"],
        "fonts": ["Arial"],
        "headers": {
          "User-Agent": "%s",
          "Accept-Language": "en"
        },
        "surface_policy": {
          "canvas": {"mode": "override"},
          "audio": {"mode": "native"},
          "client_rects": {"mode": "native"},
          "webgl": {"mode": "override"},
          "fonts": {"mode": "native_or_allowlist"},
          "plugins": {"mode": "override"},
          "media_devices": {"mode": "override"},
          "speech_voices": {"mode": "override"}
        }
      }
      %s
    })json",
                              user_agent.c_str(), user_agent.c_str(),
                              proxy_field.c_str());
  }

  void AddGenerateSuccessResponseWithProxy(const std::string& user_agent,
                                           const std::string& username,
                                           const std::string& password) {
    std::string proxy_json = base::StringPrintf(R"json({
      "host": "proxy.example.com",
      "port": 3128,
      "country": "US",
      "city": "New York",
      "username": "%s",
      "password": "%s"
    })json",
                                                username.c_str(),
                                                password.c_str());
    url_loader_factory_.AddResponse(
        std::string(kConfiguredApiBaseUrl) + "/v1/fingerprints/generate",
        GenerateSuccessResponseJson(user_agent, proxy_json));
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
#if BUILDFLAG(IS_WIN)
  std::unique_ptr<base::ScopedPathOverride> local_app_data_override_;
#else
  std::unique_ptr<base::ScopedPathOverride> home_override_;
#endif
  std::unique_ptr<base::Environment> env_;
  network::TestURLLoaderFactory url_loader_factory_;
};

TEST_F(StartupTest, VanillaMode) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitch("restore-last-session");
  cmd.AppendSwitch("no-startup-window");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->should_exit);
  ASSERT_EQ(cmd.GetArgs().size(), 1u);
  EXPECT_EQ(cmd.GetArgs()[0], FILE_PATH_LITERAL("clawbrowser://auth/"));
  EXPECT_FALSE(cmd.HasSwitch("restore-last-session"));
  EXPECT_FALSE(cmd.HasSwitch("no-startup-window"));
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_NE(cmd.GetSwitchValueASCII("user-data-dir").find("Auth"),
            std::string::npos);
  // Accessor should be null (no fingerprint)
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(StartupTest, ListProfiles) {
  WriteCachedProfile("list_profile");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitch("list");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->should_exit);
  EXPECT_EQ(result->exit_code, 0);
}

TEST_F(StartupTest, HandleBasicStartupCompleteListsProfiles) {
  WriteCachedProfile("list_profile");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitch("list");
  cmd.AppendSwitchASCII("output", "json");

  testing::internal::CaptureStdout();
  auto result = HandleBasicStartupComplete(cmd);
  std::string stdout_output = testing::internal::GetCapturedStdout();

  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_TRUE(result->has_value());
  EXPECT_EQ(result->value(), 0);
  EXPECT_NE(stdout_output.find("\"id\": \"list_profile\""), std::string::npos);
}

TEST_F(StartupTest, FingerprintWithCachedProfile) {
  WriteCachedProfile("cached_profile");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "cached_profile");
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
  EXPECT_EQ(cmd.GetSwitchValueASCII("proxy-server"),
            "http://proxy.example.com:8080");
  EXPECT_TRUE(cmd.HasSwitch("lang"));
  EXPECT_TRUE(cmd.HasSwitch("accept-lang"));
  EXPECT_TRUE(cmd.HasSwitch("user-agent"));
  EXPECT_EQ(cmd.GetSwitchValueASCII("user-agent"),
            FingerprintAccessor::Get()->user_agent);
  // User-agent client hints are no longer plumbed through command-line
  // switches: a5fc4ed replaced clawbrowser-ua-{major-version,full-version,
  // platform} with the ChromeContentBrowserClient::GetUserAgentMetadata()
  // override (patch 024) that returns BuildUserAgentMetadata(fingerprint).
  // That path is covered by clawbrowser/test/integration/test_surfaces.py
  // (test_navigator_user_agent_data, test_sec_ch_ua_headers).
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(cmd.HasSwitch(kFingerprintChildDataSwitch));
#else
  EXPECT_FALSE(cmd.HasSwitch(kFingerprintChildDataSwitch));
#endif
}

TEST_F(StartupTest, CachedNativeSurfacePolicyIsRegenerated) {
  WriteLegacyPrivacyPolicyCachedProfile("legacy_privacy_profile");
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");
  env_->SetVar("CLAWBROWSER_API_BASE_URL", kConfiguredApiBaseUrl);
  const std::string user_agent =
      "Mozilla/5.0 AppleWebKit/537.36 Chrome/120.0.0.0 Safari/537.36";
  url_loader_factory_.AddResponse(
      std::string(kConfiguredApiBaseUrl) + "/v1/fingerprints/generate",
      GenerateSuccessResponseJson(user_agent));

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "legacy_privacy_profile");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);

  ProfileEnvelope saved = ReadSavedProfile("legacy_privacy_profile");
  EXPECT_EQ(saved.response.fingerprint.surface_policy.canvas.mode, "override");
  EXPECT_EQ(saved.response.fingerprint.surface_policy.webgl.mode, "override");
}

TEST(SurfaceSpoofingResolutionTest, PolicyEnablesWithoutAnyFlag) {
  // The regression this guards: spoofing used to require --enable-*-spoofing
  // even when the profile asked for "override", so a fingerprint could carry a
  // WebGL renderer string and still emit the host's real adapter.
  EXPECT_TRUE(ResolveSurfaceSpoofing(/*forced_on=*/false, /*forced_off=*/false,
                                     /*policy_requests_override=*/true));
}

TEST(SurfaceSpoofingResolutionTest, NativePolicyStaysOff) {
  EXPECT_FALSE(ResolveSurfaceSpoofing(false, false, false));
}

TEST(SurfaceSpoofingResolutionTest, EnableSwitchForcesOnAgainstNativePolicy) {
  EXPECT_TRUE(ResolveSurfaceSpoofing(/*forced_on=*/true, /*forced_off=*/false,
                                     /*policy_requests_override=*/false));
}

TEST(SurfaceSpoofingResolutionTest, DisableSwitchBeatsEverything) {
  EXPECT_FALSE(ResolveSurfaceSpoofing(/*forced_on=*/true, /*forced_off=*/true,
                                      /*policy_requests_override=*/true));
  EXPECT_FALSE(ResolveSurfaceSpoofing(false, true, true));
}

TEST_F(StartupTest, WindowSizeFitsInsideSpoofedScreen) {
  // window.outerWidth/outerHeight come from the real OS window and are not
  // spoofed, so a window larger than the screen we advertise is a one-line
  // contradiction for any detector. The fixture reports 1920x1080 with a
  // 1920x1040 available area.
  WriteCachedProfile("cached_profile");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "cached_profile");

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);

  ASSERT_TRUE(cmd.HasSwitch("window-size"));
  const std::string value = cmd.GetSwitchValueASCII("window-size");
  const size_t comma = value.find(',');
  ASSERT_NE(comma, std::string::npos) << "window-size was: " << value;

  int width = 0;
  int height = 0;
  ASSERT_TRUE(base::StringToInt(value.substr(0, comma), &width));
  ASSERT_TRUE(base::StringToInt(value.substr(comma + 1), &height));

  const auto* fp = FingerprintAccessor::Get();
  EXPECT_GT(width, 0);
  EXPECT_GT(height, 0);
  EXPECT_LE(width, fp->screen.avail_width);
  EXPECT_LE(height, fp->screen.avail_height);
  EXPECT_LE(fp->screen.avail_width, fp->screen.width);
  EXPECT_LE(fp->screen.avail_height, fp->screen.height);

  // Anchored at the origin so screenX + outerWidth stays on-screen too.
  EXPECT_EQ(cmd.GetSwitchValueASCII("window-position"), "0,0");
}

TEST_F(StartupTest, ExplicitWindowSizeIsNotOverridden) {
  // A caller-supplied size is a deliberate, controlled override and must win.
  WriteCachedProfile("cached_profile");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "cached_profile");
  cmd.AppendSwitchASCII("window-size", "801,601");

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_EQ(cmd.GetSwitchValueASCII("window-size"), "801,601");
}

TEST_F(StartupTest, SpoofingFlagsApplyToLoadedBrowserFingerprint) {
  WriteCachedProfile("cached_profile");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "cached_profile");
  cmd.AppendSwitch(kEnableCanvasSpoofingSwitch);
  cmd.AppendSwitch(kEnableWebGLSpoofingSwitch);

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());

  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_TRUE(FingerprintAccessor::Get()->canvas_spoofing_enabled);
  EXPECT_TRUE(FingerprintAccessor::Get()->webgl_spoofing_enabled);
}

TEST_F(StartupTest,
       DevProxyUrlOverrideUsesSocks5AuthBridgeWithoutLeakingSecrets) {
#if defined(CLAWBROWSER_ENABLE_DEV_PROXY_URL_OVERRIDE) && \
    CLAWBROWSER_ENABLE_DEV_PROXY_URL_OVERRIDE
  WriteCachedProfile("cached_profile");
  WriteConfigJson("test_key");
  env_->SetVar("CLAWBROWSER_DEV_PROXY_URL",
               "socks5://dev_user:dev_pass@gate.nodemaven.com:1080");

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "cached_profile");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());

  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  ASSERT_NE(FingerprintAccessor::GetProxy(), nullptr);
  EXPECT_EQ(FingerprintAccessor::GetProxy()->scheme.value_or(""), "socks5");
  EXPECT_EQ(FingerprintAccessor::GetProxy()->host.value_or(""),
            "gate.nodemaven.com");
  EXPECT_EQ(FingerprintAccessor::GetProxy()->port.value_or(0), 1080);
  EXPECT_EQ(FingerprintAccessor::GetProxy()->username.value_or(""),
            "dev_user");
  EXPECT_EQ(FingerprintAccessor::GetProxy()->password.value_or(""),
            "dev_pass");
  ASSERT_TRUE(cmd.HasSwitch("proxy-server"));
  EXPECT_TRUE(base::StartsWith(cmd.GetSwitchValueASCII("proxy-server"),
                               "http://127.0.0.1:",
                               base::CompareCase::SENSITIVE));
  EXPECT_EQ(cmd.GetSwitchValueASCII("proxy-server").find("dev_user"),
            std::string::npos);
  EXPECT_EQ(cmd.GetSwitchValueASCII("proxy-server").find("dev_pass"),
            std::string::npos);

#if BUILDFLAG(IS_MAC)
  ASSERT_TRUE(cmd.HasSwitch(kFingerprintChildDataSwitch));
  std::string child_json;
  ASSERT_TRUE(base::Base64Decode(
      cmd.GetSwitchValueASCII(kFingerprintChildDataSwitch), &child_json));
  EXPECT_NE(child_json.find("\"scheme\":\"socks5\""), std::string::npos);
  EXPECT_NE(child_json.find("gate.nodemaven.com"), std::string::npos);
  EXPECT_EQ(child_json.find("dev_user"), std::string::npos);
  EXPECT_EQ(child_json.find("dev_pass"), std::string::npos);
#endif
#endif
}

TEST_F(StartupTest, FingerprintWithCachedProfileWithoutPrefix) {
  WriteLegacyRawCachedProfile("custom_profile");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "custom_profile");

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_TRUE(cmd.HasSwitch(kFingerprintPathSwitch));
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("custom_profile")));
}

TEST_F(StartupTest, FingerprintWithCachedProfilePathLikeId) {
  WriteCachedProfile("group/profile");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "group/profile");

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_TRUE(cmd.HasSwitch(kFingerprintPathSwitch));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("group/profile")));
}

TEST_F(StartupTest, FingerprintWithCachedProfileEmptyId) {
  WriteCachedProfile("");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "");

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_TRUE(cmd.HasSwitch(kFingerprintPathSwitch));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(CreateProfileManager().GetUserDataDir("")));
}

TEST_F(StartupTest, ConfigureEarlyStartupSetsFingerprintUserDataDir) {
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "early_profile");

  auto result = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("early_profile")));
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(StartupTest, ConfigureEarlyStartupUsesDefaultFingerprintEnv) {
  WriteConfigJson("test_key");
  env_->SetVar("CLAWBROWSER_DEFAULT_FINGERPRINT_ID", "clawbrowser_default");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);

  auto result = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  EXPECT_EQ(cmd.GetSwitchValueASCII("fingerprint"), "clawbrowser_default");
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("clawbrowser_default")));
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(StartupTest, ConfigureEarlyStartupUsesImplicitDefaultFingerprintId) {
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);

  auto result = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  EXPECT_EQ(cmd.GetSwitchValueASCII("fingerprint"), "clawbrowser_default");
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("clawbrowser_default")));
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(StartupTest, ConfigureEarlyStartupUsesCachedProfileBeforeDefault) {
  WriteCachedProfile("existing_profile");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);

  auto result = ConfigureEarlyStartup(&cmd);

  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  EXPECT_EQ(cmd.GetSwitchValueASCII("fingerprint"), "existing_profile");
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("existing_profile")));
  EXPECT_TRUE(cmd.GetArgs().empty());
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(StartupTest, ConfigureEarlyStartupRoutesCachedProfileToAuthWithoutApiKey) {
  WriteCachedProfile("existing_profile");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitch("restore-last-session");
  cmd.AppendSwitch("no-startup-window");
  cmd.AppendArg("clawbrowser://verify/");

  auto result = ConfigureEarlyStartup(&cmd);

  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  EXPECT_EQ(cmd.GetSwitchValueASCII("fingerprint"), "existing_profile");
  ASSERT_EQ(cmd.GetArgs().size(), 1u);
  EXPECT_EQ(cmd.GetArgs()[0], FILE_PATH_LITERAL("clawbrowser://auth/"));
  EXPECT_FALSE(cmd.HasSwitch("restore-last-session"));
  EXPECT_FALSE(cmd.HasSwitch("no-startup-window"));
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_NE(cmd.GetSwitchValueASCII("user-data-dir").find("Auth"),
            std::string::npos);
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(StartupTest,
       CommandLineBeforeUserDataDirUsesImplicitDefaultFingerprintProfile) {
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);

  ConfigureCommandLineBeforeUserDataDir(&cmd);

  EXPECT_EQ(cmd.GetSwitchValueASCII("fingerprint"), "clawbrowser_default");
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("clawbrowser_default")));
  EXPECT_TRUE(cmd.GetArgs().empty());
}

TEST_F(StartupTest,
       CommandLineBeforeUserDataDirPreservesFingerprintForAuthRelaunch) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);

  ConfigureCommandLineBeforeUserDataDir(&cmd);

  EXPECT_EQ(cmd.GetSwitchValueASCII("fingerprint"), "clawbrowser_default");
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_NE(cmd.GetSwitchValueASCII("user-data-dir").find("Auth"),
            std::string::npos);
  ASSERT_EQ(cmd.GetArgs().size(), 1u);
  EXPECT_EQ(cmd.GetArgs()[0], FILE_PATH_LITERAL("clawbrowser://auth/"));
}

TEST_F(StartupTest, ConfigureEarlyStartupEnablesMockKeychainOnMac) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);

  auto result = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(cmd.HasSwitch("use-mock-keychain"));
#else
  EXPECT_FALSE(cmd.HasSwitch("use-mock-keychain"));
#endif
}

TEST_F(StartupTest,
       ConfigureEarlyStartupSetsFingerprintUserDataDirWithoutPrefix) {
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "custom_profile");

  auto result = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("custom_profile")));
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(StartupTest, ConfigureEarlyStartupSetsPathLikeFingerprintUserDataDir) {
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "group/profile");

  auto result = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("group/profile")));
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(StartupTest, ConfigureEarlyStartupSetsEmptyFingerprintUserDataDir) {
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "");

  auto result = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(CreateProfileManager().GetUserDataDir("")));
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(StartupTest, ConfigureEarlyStartupFallsBackToDefaultWithoutApiKey) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "early_profile");
  cmd.AppendSwitch("restore-last-session");
  cmd.AppendSwitch("no-startup-window");
  cmd.AppendArg("clawbrowser://verify/");

  auto result = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  ASSERT_EQ(cmd.GetArgs().size(), 1u);
  EXPECT_EQ(cmd.GetArgs()[0], FILE_PATH_LITERAL("clawbrowser://auth/"));
  EXPECT_FALSE(cmd.HasSwitch("restore-last-session"));
  EXPECT_FALSE(cmd.HasSwitch("no-startup-window"));
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_NE(cmd.GetSwitchValueASCII("user-data-dir").find("Auth"),
            std::string::npos);
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
}

TEST_F(StartupTest, FingerprintCachedProfileWithoutApiKey) {
  WriteCachedProfile("cached_profile");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "cached_profile");
  cmd.AppendSwitch("restore-last-session");
  cmd.AppendSwitch("no-startup-window");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
  EXPECT_FALSE(cmd.HasSwitch(kFingerprintPathSwitch));
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_NE(cmd.GetSwitchValueASCII("user-data-dir").find("Auth"),
            std::string::npos);
  ASSERT_EQ(cmd.GetArgs().size(), 1u);
  EXPECT_EQ(cmd.GetArgs()[0], FILE_PATH_LITERAL("clawbrowser://auth/"));
  EXPECT_FALSE(cmd.HasSwitch("restore-last-session"));
  EXPECT_FALSE(cmd.HasSwitch("no-startup-window"));
  EXPECT_FALSE(cmd.HasSwitch("proxy-server"));
}

TEST_F(StartupTest, FingerprintNoApiKeyWithoutCachedProfileOpensAuth) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "new_profile");
  cmd.AppendSwitch("restore-last-session");
  cmd.AppendSwitch("no-startup-window");
  cmd.AppendArg("clawbrowser://verify/");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->should_exit);
  ASSERT_EQ(cmd.GetArgs().size(), 1u);
  EXPECT_EQ(cmd.GetArgs()[0], FILE_PATH_LITERAL("clawbrowser://auth/"));
  EXPECT_FALSE(cmd.HasSwitch("restore-last-session"));
  EXPECT_FALSE(cmd.HasSwitch("no-startup-window"));
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_NE(cmd.GetSwitchValueASCII("user-data-dir").find("Auth"),
            std::string::npos);
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
  EXPECT_FALSE(cmd.HasSwitch("proxy-server"));
}

TEST_F(StartupTest, FingerprintApiCallSuccess) {
  WriteConfigJson("test_key");
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");
  env_->SetVar("CLAWBROWSER_API_BASE_URL", kConfiguredApiBaseUrl);

  // Mock API response
  std::string api_response = R"({
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
      std::string(kConfiguredApiBaseUrl) + "/v1/fingerprints/generate",
      api_response);

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "api_profile");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_EQ(FingerprintAccessor::Get()->user_agent, "test-ua");
  ProfileEnvelope saved = ReadSavedProfile("api_profile");
  EXPECT_EQ(saved.request.browser, "chrome");
}

TEST_F(StartupTest, FreshStartWithApiKeyRunsInImplicitFingerprintMode) {
  WriteConfigJson("test_key");
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");
  env_->SetVar("CLAWBROWSER_API_BASE_URL", kConfiguredApiBaseUrl);

  url_loader_factory_.AddResponse(
      std::string(kConfiguredApiBaseUrl) + "/v1/fingerprints/generate",
      R"({
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
          "user_agent": "default-profile-ua", "platform": "MacIntel",
          "screen": {"width": 1920, "height": 1080, "avail_width": 1920,
                     "avail_height": 1040, "color_depth": 24, "pixel_ratio": 1.0},
          "hardware": {"concurrency": 8, "memory": 8},
          "webgl": {"vendor": "v", "renderer": "r"},
          "canvas_seed": 1, "audio_seed": 2, "client_rects_seed": 3,
          "timezone": "UTC", "language": ["en"], "fonts": ["Arial"]
        }
      })");

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  auto early = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(early.has_value()) << early.error();
  EXPECT_FALSE(early->should_exit);
  EXPECT_EQ(cmd.GetSwitchValueASCII("fingerprint"), "clawbrowser_default");
  EXPECT_TRUE(cmd.HasSwitch("user-data-dir"));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("clawbrowser_default")));

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_EQ(FingerprintAccessor::Get()->user_agent, "default-profile-ua");
  ProfileEnvelope saved = ReadSavedProfile("clawbrowser_default");
  EXPECT_EQ(saved.request.browser, "chrome");
  EXPECT_TRUE(cmd.HasSwitch(kFingerprintPathSwitch));
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("clawbrowser_default")));
  ASSERT_EQ(cmd.GetArgs().size(), 1u);
  EXPECT_EQ(cmd.GetArgs()[0], FILE_PATH_LITERAL("clawbrowser://verify/"));
}

TEST_F(StartupTest, FingerprintApiCallSuccessWithPathLikeId) {
  WriteConfigJson("test_key");
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");
  env_->SetVar("CLAWBROWSER_API_BASE_URL", kConfiguredApiBaseUrl);

  url_loader_factory_.AddResponse(
      std::string(kConfiguredApiBaseUrl) + "/v1/fingerprints/generate",
      R"({
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
          "user_agent": "pathlike-ua", "platform": "test",
          "screen": {"width": 1920, "height": 1080, "avail_width": 1920,
                     "avail_height": 1040, "color_depth": 24, "pixel_ratio": 1.0},
          "hardware": {"concurrency": 8, "memory": 8},
          "webgl": {"vendor": "test", "renderer": "test"},
          "canvas_seed": 123, "audio_seed": 456, "client_rects_seed": 789,
          "timezone": "UTC", "language": ["en"], "fonts": ["Arial"]
        }
      })");

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "group/profile");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_EQ(FingerprintAccessor::Get()->user_agent, "pathlike-ua");
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("group/profile")));
}

TEST_F(StartupTest, FingerprintApiCallSuccessWithEmptyId) {
  WriteConfigJson("test_key");
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");
  env_->SetVar("CLAWBROWSER_API_BASE_URL", kConfiguredApiBaseUrl);

  url_loader_factory_.AddResponse(
      std::string(kConfiguredApiBaseUrl) + "/v1/fingerprints/generate",
      R"({
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
          "user_agent": "empty-id-ua", "platform": "test",
          "screen": {"width": 1920, "height": 1080, "avail_width": 1920,
                     "avail_height": 1040, "color_depth": 24, "pixel_ratio": 1.0},
          "hardware": {"concurrency": 8, "memory": 8},
          "webgl": {"vendor": "test", "renderer": "test"},
          "canvas_seed": 123, "audio_seed": 456, "client_rects_seed": 789,
          "timezone": "UTC", "language": ["en"], "fonts": ["Arial"]
        }
      })");

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_EQ(FingerprintAccessor::Get()->user_agent, "empty-id-ua");
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(CreateProfileManager().GetUserDataDir("")));
}

TEST_F(StartupTest, FingerprintApiCallWithoutConfiguredBaseUrlRespectsBuildDefault) {
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");

  const auto build_default = GetBuildDefaultApiBaseUrl();
  if (build_default.has_value()) {
    url_loader_factory_.AddResponse(
        *build_default + "/v1/fingerprints/generate",
        R"({
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
            "user_agent": "default-ua", "platform": "test",
            "screen": {"width": 1920, "height": 1080, "avail_width": 1920,
                       "avail_height": 1040, "color_depth": 24, "pixel_ratio": 1.0},
            "hardware": {"concurrency": 8, "memory": 8},
            "webgl": {"vendor": "test", "renderer": "test"},
            "canvas_seed": 123, "audio_seed": 456, "client_rects_seed": 789,
            "timezone": "UTC", "language": ["en"], "fonts": ["Arial"]
          }
        })");
  }

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "missing_base_url_profile");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  if (build_default.has_value()) {
    EXPECT_FALSE(result->should_exit);
    ASSERT_NE(FingerprintAccessor::Get(), nullptr);
    EXPECT_EQ(FingerprintAccessor::Get()->user_agent, "default-ua");
    return;
  }

  EXPECT_TRUE(result->should_exit);
  EXPECT_EQ(result->exit_code, 1);
}

TEST_F(StartupTest, FingerprintApiCall401) {
  env_->SetVar("CLAWBROWSER_API_KEY", "bad_key");
  env_->SetVar("CLAWBROWSER_API_BASE_URL", kConfiguredApiBaseUrl);

  AddGenerateErrorResponse(
      R"({"code": "invalid_api_key", "message": "invalid API key"})",
      net::HTTP_UNAUTHORIZED);

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "bad_key_profile");
  cmd.AppendSwitch("restore-last-session");
  cmd.AppendSwitch("no-startup-window");
  cmd.AppendArg("clawbrowser://verify/");
  auto early = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(early.has_value()) << early.error();
  EXPECT_FALSE(early->should_exit);
  EXPECT_EQ(UserDataDirSwitch(cmd),
            NormalizePathForComparison(
                CreateProfileManager().GetUserDataDir("bad_key_profile")));

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->should_exit);
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
  EXPECT_FALSE(cmd.HasSwitch(kFingerprintPathSwitch));
  EXPECT_FALSE(cmd.HasSwitch("proxy-server"));
  EXPECT_FALSE(cmd.HasSwitch("restore-last-session"));
  EXPECT_FALSE(cmd.HasSwitch("no-startup-window"));
  ASSERT_EQ(cmd.GetArgs().size(), 1u);
  EXPECT_EQ(cmd.GetArgs()[0], FILE_PATH_LITERAL("clawbrowser://auth/"));
  EXPECT_NE(cmd.GetSwitchValueASCII("user-data-dir").find("Auth"),
            std::string::npos);
}

TEST_F(StartupTest, FingerprintApiCall403OpensAuth) {
  env_->SetVar("CLAWBROWSER_API_KEY", "forbidden_key");
  env_->SetVar("CLAWBROWSER_API_BASE_URL", kConfiguredApiBaseUrl);

  AddGenerateErrorResponse(
      R"({"code": "forbidden", "message": "invalid API key"})",
      net::HTTP_FORBIDDEN);

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "forbidden_key_profile");
  auto early = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(early.has_value()) << early.error();
  EXPECT_FALSE(early->should_exit);

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->should_exit);
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
  EXPECT_FALSE(cmd.HasSwitch(kFingerprintPathSwitch));
  EXPECT_FALSE(cmd.HasSwitch("proxy-server"));
  ASSERT_EQ(cmd.GetArgs().size(), 1u);
  EXPECT_EQ(cmd.GetArgs()[0], FILE_PATH_LITERAL("clawbrowser://auth/"));
  EXPECT_NE(cmd.GetSwitchValueASCII("user-data-dir").find("Auth"),
            std::string::npos);
}

TEST_F(StartupTest, FingerprintApiCall500FallsBackWithoutAuth) {
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");
  env_->SetVar("CLAWBROWSER_API_BASE_URL", kConfiguredApiBaseUrl);

  AddGenerateErrorResponse(
      R"({"code": "server_error", "message": "upstream failed"})",
      net::HTTP_INTERNAL_SERVER_ERROR);

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "server_error_profile");
  auto early = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(early.has_value()) << early.error();
  EXPECT_FALSE(early->should_exit);

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->should_exit);
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
  EXPECT_FALSE(cmd.HasSwitch(kFingerprintPathSwitch));
  EXPECT_FALSE(cmd.HasSwitch("proxy-server"));
  EXPECT_TRUE(cmd.GetArgs().empty());
  EXPECT_NE(cmd.GetSwitchValueASCII("user-data-dir").find("Default"),
            std::string::npos);
}

TEST_F(StartupTest, FingerprintApiNetworkFailureFallsBackWithoutAuth) {
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");
  env_->SetVar("CLAWBROWSER_API_BASE_URL", kConfiguredApiBaseUrl);

  url_loader_factory_.AddResponse(
      GURL(std::string(kConfiguredApiBaseUrl) + "/v1/fingerprints/generate"),
      network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_TIMED_OUT));

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "network_failure_profile");
  auto early = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(early.has_value()) << early.error();
  EXPECT_FALSE(early->should_exit);

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->should_exit);
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
  EXPECT_FALSE(cmd.HasSwitch(kFingerprintPathSwitch));
  EXPECT_FALSE(cmd.HasSwitch("proxy-server"));
  EXPECT_TRUE(cmd.GetArgs().empty());
  EXPECT_NE(cmd.GetSwitchValueASCII("user-data-dir").find("Default"),
            std::string::npos);
}

TEST_F(StartupTest,
       FingerprintApiLargeSeedParseFailureFallsBackWithoutAuth) {
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");
  env_->SetVar("CLAWBROWSER_API_BASE_URL", kConfiguredApiBaseUrl);

  url_loader_factory_.AddResponse(
      std::string(kConfiguredApiBaseUrl) + "/v1/fingerprints/generate",
      R"json({
        "fingerprint": {
          "user_agent": "test-ua", "platform": "MacIntel",
          "screen": {"width": 1512, "height": 982, "avail_width": 1512,
                     "avail_height": 949, "color_depth": 30, "pixel_ratio": 2.0},
          "hardware": {"concurrency": 4, "memory": 8},
          "webgl": {"vendor": "Google Inc. (Apple)", "renderer": "ANGLE"},
          "canvas_seed": 321187992662224890,
          "audio_seed": 1089650042381430990,
          "client_rects_seed": 4186367778024628107,
          "timezone": "America/Chicago",
          "language": ["en-US", "en"],
          "fonts": ["Arial Unicode MS", "Gill Sans", "Helvetica Neue", "Menlo"]
        }
      })json");

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "large_seed_failure_profile");
  auto early = ConfigureEarlyStartup(&cmd);
  ASSERT_TRUE(early.has_value()) << early.error();
  EXPECT_FALSE(early->should_exit);

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->should_exit);
  EXPECT_EQ(FingerprintAccessor::Get(), nullptr);
  EXPECT_FALSE(cmd.HasSwitch(kFingerprintPathSwitch));
  EXPECT_FALSE(cmd.HasSwitch("proxy-server"));
  EXPECT_TRUE(cmd.GetArgs().empty());
  EXPECT_NE(cmd.GetSwitchValueASCII("user-data-dir").find("Default"),
            std::string::npos);
}

TEST_F(StartupTest, RegenerateReplaysStoredParams) {
  WriteCachedProfile("regen_profile");
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");
  env_->SetVar("CLAWBROWSER_API_BASE_URL", kConfiguredApiBaseUrl);

  // Mock API — we just need it to succeed
  url_loader_factory_.AddResponse(
      std::string(kConfiguredApiBaseUrl) + "/v1/fingerprints/generate",
      R"({
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
  cmd.AppendSwitchASCII("fingerprint", "regen_profile");
  cmd.AppendSwitch("regenerate");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  // New fingerprint should be loaded
  ASSERT_NE(FingerprintAccessor::Get(), nullptr);
  EXPECT_EQ(FingerprintAccessor::Get()->user_agent, "new-ua");
}

TEST_F(StartupTest, RegenerateOverwritesEncryptedProxyCredentials) {
  WriteCachedProfileWithProxy("regen_proxy_profile", "stale_user", "stale_pass");
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");
  env_->SetVar("CLAWBROWSER_API_BASE_URL", kConfiguredApiBaseUrl);
  AddGenerateSuccessResponseWithProxy("fresh-ua", "fresh_user", "fresh_pass");

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "regen_proxy_profile");
  cmd.AppendSwitch("regenerate");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);
  ASSERT_NE(FingerprintAccessor::GetProxy(), nullptr);
  EXPECT_EQ(FingerprintAccessor::GetProxy()->username.value_or(""),
            "fresh_user");
  EXPECT_EQ(FingerprintAccessor::GetProxy()->password.value_or(""),
            "fresh_pass");

  ProfileEnvelope saved = ReadSavedProfile("regen_proxy_profile");
  ASSERT_TRUE(saved.response.proxy.has_value());
  EXPECT_EQ(saved.response.proxy->username.value_or(""), "fresh_user");
  EXPECT_EQ(saved.response.proxy->password.value_or(""), "fresh_pass");

  std::string raw_json;
  base::FilePath path =
      CreateProfileManager().GetFingerprintPath("regen_proxy_profile");
  ASSERT_TRUE(base::ReadFileToString(path, &raw_json));
  EXPECT_EQ(raw_json.find("fresh_user"), std::string::npos);
  EXPECT_EQ(raw_json.find("fresh_pass"), std::string::npos);
}

TEST_F(StartupTest, FingerprintApiCallUsesLocationOverrides) {
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");
  env_->SetVar("CLAWBROWSER_API_BASE_URL", kConfiguredApiBaseUrl);

  url_loader_factory_.AddResponse(
      std::string(kConfiguredApiBaseUrl) + "/v1/fingerprints/generate",
      R"({
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
          "user_agent": "test-ua", "platform": "MacIntel",
          "screen": {"width": 1920, "height": 1080, "avail_width": 1920,
                     "avail_height": 1040, "color_depth": 24, "pixel_ratio": 1.0},
          "hardware": {"concurrency": 8, "memory": 8},
          "webgl": {"vendor": "v", "renderer": "r"},
          "canvas_seed": 1, "audio_seed": 2, "client_rects_seed": 3,
          "timezone": "Europe/Berlin", "language": ["de-DE"], "fonts": ["Arial"]
        }
      })");

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "geo_profile");
  cmd.AppendSwitchASCII("country", "DE");
  cmd.AppendSwitchASCII("city", "Berlin");
  cmd.AppendSwitchASCII("connection-type", "mobile");
  cmd.AppendSwitchASCII("proxy-scheme", "socks5");

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);

  ProfileEnvelope saved = ReadSavedProfile("geo_profile");
  EXPECT_EQ(saved.request.country, "DE");
  ASSERT_TRUE(saved.request.city.has_value());
  EXPECT_EQ(*saved.request.city, "Berlin");
  ASSERT_TRUE(saved.request.connection_type.has_value());
  EXPECT_EQ(*saved.request.connection_type, "mobile");
  ASSERT_TRUE(saved.request.proxy_scheme.has_value());
  EXPECT_EQ(*saved.request.proxy_scheme, "socks5");
}

TEST_F(StartupTest, FingerprintApiCallSendsRuntimeHintsFromLaunchFlags) {
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");
  env_->SetVar("CLAWBROWSER_API_BASE_URL", kConfiguredApiBaseUrl);

  const std::string user_agent =
      "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
      "AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/120.0.0.0 Safari/537.36";
  url_loader_factory_.AddResponse(
      std::string(kConfiguredApiBaseUrl) + "/v1/fingerprints/generate",
      GenerateSuccessResponseJson(user_agent));

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "runtime_profile");
  cmd.AppendSwitch("disable-gpu");
  cmd.AppendSwitchASCII("headless", "new");

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);

  ProfileEnvelope saved = ReadSavedProfile("runtime_profile");
  ASSERT_TRUE(saved.request.runtime_browser_version.has_value());
  EXPECT_FALSE(saved.request.runtime_browser_version->empty());
  ASSERT_TRUE(saved.request.runtime_os.has_value());
  EXPECT_FALSE(saved.request.runtime_os->empty());
  EXPECT_FALSE(saved.request.runtime_os_version.has_value());
  ASSERT_TRUE(saved.request.runtime_arch.has_value());
  EXPECT_FALSE(saved.request.runtime_arch->empty());
  ASSERT_TRUE(saved.request.runtime_gpu.has_value());
  EXPECT_EQ(*saved.request.runtime_gpu, "swiftshader");
  ASSERT_TRUE(saved.request.runtime_headless.has_value());
  EXPECT_TRUE(*saved.request.runtime_headless);
}

TEST_F(StartupTest, FingerprintApiCallSendsNativeDesktopGPUHint) {
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");
  env_->SetVar("CLAWBROWSER_API_BASE_URL", kConfiguredApiBaseUrl);

  const std::string user_agent =
      "Mozilla/5.0 AppleWebKit/537.36 Chrome/120.0.0.0 Safari/537.36";
  url_loader_factory_.AddResponse(
      std::string(kConfiguredApiBaseUrl) + "/v1/fingerprints/generate",
      GenerateSuccessResponseJson(user_agent));

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "native_gpu_profile");

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);

  ProfileEnvelope saved = ReadSavedProfile("native_gpu_profile");
#if BUILDFLAG(IS_MAC)
  ASSERT_TRUE(saved.request.runtime_gpu.has_value());
  EXPECT_EQ(*saved.request.runtime_gpu, "apple-metal");
#elif BUILDFLAG(IS_WIN)
  ASSERT_TRUE(saved.request.runtime_gpu.has_value());
  EXPECT_EQ(*saved.request.runtime_gpu, "direct3d11");
#else
  EXPECT_FALSE(saved.request.runtime_gpu.has_value());
#endif
  ASSERT_TRUE(saved.request.runtime_headless.has_value());
  EXPECT_FALSE(*saved.request.runtime_headless);
}

TEST_F(StartupTest, FingerprintApiCallAllowsCityOnlyOverrides) {
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");
  env_->SetVar("CLAWBROWSER_API_BASE_URL", kConfiguredApiBaseUrl);

  url_loader_factory_.AddResponse(
      std::string(kConfiguredApiBaseUrl) + "/v1/fingerprints/generate",
      R"({
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
          "user_agent": "test-ua", "platform": "MacIntel",
          "screen": {"width": 1920, "height": 1080, "avail_width": 1920,
                     "avail_height": 1040, "color_depth": 24, "pixel_ratio": 1.0},
          "hardware": {"concurrency": 8, "memory": 8},
          "webgl": {"vendor": "v", "renderer": "r"},
          "canvas_seed": 1, "audio_seed": 2, "client_rects_seed": 3,
          "timezone": "Europe/Berlin", "language": ["de-DE"], "fonts": ["Arial"]
        }
      })");

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "city_only_profile");
  cmd.AppendSwitchASCII("city", "Berlin");

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);

  ProfileEnvelope saved = ReadSavedProfile("city_only_profile");
  EXPECT_TRUE(saved.request.country.empty());
  ASSERT_TRUE(saved.request.city.has_value());
  EXPECT_EQ(*saved.request.city, "Berlin");
  EXPECT_FALSE(saved.request.connection_type.has_value());
}

TEST_F(StartupTest, RegenerateCountryOverrideClearsStaleOptionalTargeting) {
  GenerateRequest request;
  request.platform = "macos";
  request.browser = "clawbrowser";
  request.country = "DE";
  request.city = "Berlin";
  request.connection_type = "mobile";
  WriteCachedProfileWithRequest("regen_targeting_profile", request);
  env_->SetVar("CLAWBROWSER_API_KEY", "test_key");
  env_->SetVar("CLAWBROWSER_API_BASE_URL", kConfiguredApiBaseUrl);

  url_loader_factory_.AddResponse(
      std::string(kConfiguredApiBaseUrl) + "/v1/fingerprints/generate",
      R"({
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
          "user_agent": "test-ua", "platform": "MacIntel",
          "screen": {"width": 1920, "height": 1080, "avail_width": 1920,
                     "avail_height": 1040, "color_depth": 24, "pixel_ratio": 1.0},
          "hardware": {"concurrency": 8, "memory": 8},
          "webgl": {"vendor": "v", "renderer": "r"},
          "canvas_seed": 1, "audio_seed": 2, "client_rects_seed": 3,
          "timezone": "UTC", "language": ["en"], "fonts": ["Arial"]
        }
      })");

  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "regen_targeting_profile");
  cmd.AppendSwitch("regenerate");
  cmd.AppendSwitchASCII("country", "US");

  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_FALSE(result->should_exit);

  ProfileEnvelope saved = ReadSavedProfile("regen_targeting_profile");
  EXPECT_EQ(saved.request.browser, "clawbrowser");
  EXPECT_EQ(saved.request.country, "US");
  EXPECT_FALSE(saved.request.city.has_value());
  EXPECT_FALSE(saved.request.connection_type.has_value());
}

TEST_F(StartupTest, VerboseLogging) {
  WriteCachedProfile("verbose_profile");
  WriteConfigJson("test_key");
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "verbose_profile");
  cmd.AppendSwitch("verbose");
  auto result = RunStartup(&cmd, url_loader_factory_.GetSafeWeakWrapper());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(IsVerbose());
}

TEST_F(StartupTest, MultipleFingerprintFlagsLastWins) {
  // When --fingerprint is specified multiple times, last value wins
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "first_profile");
  cmd.AppendSwitchASCII("fingerprint", "second_profile");

  ClawArgs args = ClawArgs::Parse(cmd);
  // base::CommandLine last-wins semantics for duplicate switches
  EXPECT_EQ(args.fingerprint_id(), "second_profile");
}

}  // namespace
}  // namespace clawbrowser
