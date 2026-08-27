#include "clawbrowser/auth/auth_page.h"

#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

TEST(AuthPageTest, DashboardUrlMatchesBuildDefault) {
#if defined(CLAWBROWSER_DEFAULT_DASHBOARD_URL)
  EXPECT_EQ(GetAuthDashboardUrl(), CLAWBROWSER_DEFAULT_DASHBOARD_URL);
#else
  EXPECT_EQ(GetAuthDashboardUrl(), "https://app.clawbrowser.ai");
#endif
}

TEST(AuthPageTest, SaveSuccessMessageExplainsAutomaticRestart) {
  EXPECT_EQ(GetAuthRestartMessage(),
            "API key saved. Restarting Clawbrowser...");
}

TEST(AuthPageTest, AuthHostConstantMatchesStartupUrl) {
  EXPECT_STREQ(kAuthHost, "auth");
}

TEST(AuthPageTest,
     RelaunchCommandLineDropsAuthVerifyUrlsAndUserDataDirPreservesOtherArgs) {
  base::CommandLine current_command_line(
      base::FilePath::FromUTF8Unsafe("/tmp/Clawbrowser.real"));
  current_command_line.AppendSwitchASCII("fingerprint", "backend_verify");
  current_command_line.AppendSwitch("regenerate");
  current_command_line.AppendSwitchASCII("user-data-dir", "/tmp/vanilla");
  current_command_line.AppendSwitch("restart");
  current_command_line.AppendSwitch("restore-last-session");
  current_command_line.AppendArg("clawbrowser://auth/");
  current_command_line.AppendArg("clawbrowser://verify/");
  current_command_line.AppendArg("https://example.com/welcome");

  base::CommandLine relaunch_command_line =
      BuildAuthRelaunchCommandLine(current_command_line);

  EXPECT_EQ(relaunch_command_line.GetProgram(),
            current_command_line.GetProgram());
  EXPECT_EQ(relaunch_command_line.GetSwitchValueASCII("fingerprint"),
            "backend_verify");
  EXPECT_TRUE(relaunch_command_line.HasSwitch("regenerate"));
  EXPECT_FALSE(relaunch_command_line.HasSwitch("user-data-dir"));
  EXPECT_FALSE(relaunch_command_line.HasSwitch("restart"));
  EXPECT_FALSE(relaunch_command_line.HasSwitch("restore-last-session"));
  ASSERT_EQ(relaunch_command_line.GetArgs().size(), 1u);
  EXPECT_EQ(relaunch_command_line.GetArgs()[0],
            FILE_PATH_LITERAL("https://example.com/welcome"));
}

TEST(AuthPageTest, RelaunchCommandLineForcesRegenerateInFingerprintMode) {
  base::CommandLine current_command_line(
      base::FilePath::FromUTF8Unsafe("/tmp/Clawbrowser.real"));
  current_command_line.AppendSwitchASCII("fingerprint", "clawbrowser_default");

  base::CommandLine relaunch_command_line =
      BuildAuthRelaunchCommandLine(current_command_line);

  EXPECT_EQ(relaunch_command_line.GetSwitchValueASCII("fingerprint"),
            "clawbrowser_default");
  EXPECT_TRUE(relaunch_command_line.HasSwitch("regenerate"));
}

TEST(AuthPageTest, RelaunchCommandLineUsesImplicitDefaultFingerprintFallback) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto env = base::Environment::Create();
  env->SetVar("CLAWBROWSER_CONFIG_DIR", temp_dir.GetPath().AsUTF8Unsafe());
  env->UnSetVar("CLAWBROWSER_DEFAULT_FINGERPRINT_ID");

  base::CommandLine current_command_line(
      base::FilePath::FromUTF8Unsafe("/tmp/Clawbrowser.real"));
  current_command_line.AppendArg("clawbrowser://auth/");

  base::CommandLine relaunch_command_line =
      BuildAuthRelaunchCommandLine(current_command_line);

  env->UnSetVar("CLAWBROWSER_CONFIG_DIR");
  EXPECT_EQ(relaunch_command_line.GetSwitchValueASCII("fingerprint"),
            "clawbrowser_default");
  EXPECT_TRUE(relaunch_command_line.HasSwitch("regenerate"));
}

TEST(AuthPageTest, RelaunchCommandLineUsesCachedProfileBeforeDefaultFallback) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath profile_dir = temp_dir.GetPath()
                                   .AppendASCII("Browser")
                                   .AppendASCII("cached_profile");
  ASSERT_TRUE(base::CreateDirectory(profile_dir));
  ASSERT_TRUE(base::WriteFile(profile_dir.AppendASCII("fingerprint.json"), R"({
      "schema_version": 1,
      "created_at": "2026-03-23T10:00:00Z",
      "profile_id": "cached_profile",
      "request": {"platform": "macos", "browser": "chrome", "country": "US"},
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
    })"));

  auto env = base::Environment::Create();
  env->SetVar("CLAWBROWSER_CONFIG_DIR", temp_dir.GetPath().AsUTF8Unsafe());
  env->UnSetVar("CLAWBROWSER_DEFAULT_FINGERPRINT_ID");

  base::CommandLine current_command_line(
      base::FilePath::FromUTF8Unsafe("/tmp/Clawbrowser.real"));
  current_command_line.AppendArg("clawbrowser://auth/");

  base::CommandLine relaunch_command_line =
      BuildAuthRelaunchCommandLine(current_command_line);

  env->UnSetVar("CLAWBROWSER_CONFIG_DIR");
  EXPECT_EQ(relaunch_command_line.GetSwitchValueASCII("fingerprint"),
            "cached_profile");
  EXPECT_TRUE(relaunch_command_line.HasSwitch("regenerate"));
}

TEST(AuthPageTest,
     RelaunchCommandLineUsesDefaultFingerprintFromBundleEnvironmentOnMac) {
#if BUILDFLAG(IS_MAC)
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto env = base::Environment::Create();
  env->SetVar("CLAWBROWSER_CONFIG_DIR", temp_dir.GetPath().AsUTF8Unsafe());
  env->UnSetVar("CLAWBROWSER_DEFAULT_FINGERPRINT_ID");

  base::FilePath app_dir = temp_dir.GetPath().AppendASCII("Clawbrowser.app");
  base::FilePath contents_dir = app_dir.AppendASCII("Contents");
  base::FilePath macos_dir = contents_dir.AppendASCII("MacOS");
  ASSERT_TRUE(base::CreateDirectory(macos_dir));

  const base::FilePath program_path = macos_dir.AppendASCII("Clawbrowser");
  ASSERT_TRUE(base::WriteFile(program_path, ""));

  const base::FilePath plist_path = contents_dir.AppendASCII("Info.plist");
  const std::string plist = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
 "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>LSEnvironment</key>
  <dict>
    <key>CLAWBROWSER_DEFAULT_FINGERPRINT_ID</key>
    <string>clawbrowser_default</string>
  </dict>
</dict>
</plist>
)";
  ASSERT_TRUE(base::WriteFile(plist_path, plist));

  base::CommandLine current_command_line(program_path);
  current_command_line.AppendArg("clawbrowser://auth/");

  base::CommandLine relaunch_command_line =
      BuildAuthRelaunchCommandLine(current_command_line);

  env->UnSetVar("CLAWBROWSER_CONFIG_DIR");
  EXPECT_EQ(relaunch_command_line.GetSwitchValueASCII("fingerprint"),
            "clawbrowser_default");
  EXPECT_TRUE(relaunch_command_line.HasSwitch("regenerate"));
#else
  GTEST_SKIP() << "bundle environment fallback is macOS-specific";
#endif
}

TEST(AuthPageTest, EmptyApiKeyIsRejected) {
  EXPECT_FALSE(IsValidApiKeyInput(""));
  EXPECT_FALSE(IsValidApiKeyInput("   "));
  EXPECT_TRUE(IsValidApiKeyInput("cb_live_123"));
}

}  // namespace
}  // namespace clawbrowser
