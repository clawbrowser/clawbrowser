#include "clawbrowser/auth/auth_page.h"

#include <cstring>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "clawbrowser/defaults.h"
#include "clawbrowser/cli/profile_manager.h"
#include "clawbrowser/grit/clawbrowser_verify_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(IS_MAC)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace clawbrowser {

namespace {

base::FilePath GetConfigDir() {
  auto env = base::Environment::Create();
  if (std::optional<std::string> override =
          env->GetVar("CLAWBROWSER_CONFIG_DIR");
      override.has_value() && !override->empty()) {
    return base::FilePath::FromUTF8Unsafe(*override);
  }

  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);
  return home_dir.AppendASCII(".config/clawbrowser");
}

base::DictValue MakeSaveResult(bool success, const std::string& message,
                               bool restart = false) {
  base::DictValue result;
  result.Set("success", success);
  result.Set("message", message);
  result.Set("restart", restart);
  return result;
}

base::DictValue MakeRestartFailureResult(const std::string& message) {
  base::DictValue result;
  result.Set("success", false);
  result.Set("message", message);
  return result;
}

bool ShouldCopySwitchForAuthRelaunch(std::string_view switch_name) {
  return switch_name != "restart" &&
         switch_name != "restore-last-session" &&
         switch_name != "user-data-dir" &&
         switch_name != "no-startup-window";
}

bool ShouldCopyArgForAuthRelaunch(
    const base::CommandLine::StringType& arg) {
  return arg != FILE_PATH_LITERAL("clawbrowser://auth/") &&
         arg != FILE_PATH_LITERAL("clawbrowser://verify/");
}

std::optional<std::string> ResolveDefaultFingerprintFromEnvironment() {
  auto env = base::Environment::Create();
  if (std::optional<std::string> default_fingerprint =
          env->GetVar("CLAWBROWSER_DEFAULT_FINGERPRINT_ID");
      default_fingerprint.has_value() && !default_fingerprint->empty()) {
    return default_fingerprint;
  }
  return std::nullopt;
}

#if BUILDFLAG(IS_MAC)
std::optional<std::string> CopyCfStringToUtf8(CFStringRef cf_string) {
  if (!cf_string) {
    return std::nullopt;
  }

  const CFIndex max_size =
      CFStringGetMaximumSizeForEncoding(CFStringGetLength(cf_string),
                                        kCFStringEncodingUTF8) +
      1;
  std::string utf8(max_size, '\0');
  if (!CFStringGetCString(cf_string, utf8.data(), max_size,
                          kCFStringEncodingUTF8)) {
    return std::nullopt;
  }

  utf8.resize(std::strlen(utf8.c_str()));
  if (utf8.empty()) {
    return std::nullopt;
  }

  return utf8;
}

std::optional<std::string> ResolveDefaultFingerprintFromBundle(
    const base::FilePath& executable_path) {
  const base::FilePath macos_dir = executable_path.DirName();
  const base::FilePath contents_dir = macos_dir.DirName();
  if (contents_dir.BaseName().AsUTF8Unsafe() != "Contents") {
    return std::nullopt;
  }

  std::string plist_data;
  if (!base::ReadFileToString(contents_dir.AppendASCII("Info.plist"),
                              &plist_data)) {
    return std::nullopt;
  }

  CFDataRef plist_bytes = CFDataCreate(
      kCFAllocatorDefault,
      reinterpret_cast<const UInt8*>(plist_data.data()),
      plist_data.size());
  if (!plist_bytes) {
    return std::nullopt;
  }

  CFPropertyListFormat format = kCFPropertyListXMLFormat_v1_0;
  CFErrorRef error = nullptr;
  CFPropertyListRef plist = CFPropertyListCreateWithData(
      kCFAllocatorDefault, plist_bytes, kCFPropertyListImmutable, &format,
      &error);
  CFRelease(plist_bytes);
  if (error) {
    CFRelease(error);
  }
  if (!plist) {
    return std::nullopt;
  }

  std::optional<std::string> default_fingerprint;
  if (CFGetTypeID(plist) == CFDictionaryGetTypeID()) {
    const auto* bundle_dict = static_cast<CFDictionaryRef>(plist);
    CFTypeRef environment_ref =
        CFDictionaryGetValue(bundle_dict, CFSTR("LSEnvironment"));
    if (environment_ref &&
        CFGetTypeID(environment_ref) == CFDictionaryGetTypeID()) {
      const auto* environment_dict =
          static_cast<CFDictionaryRef>(environment_ref);
      CFTypeRef fingerprint_ref = CFDictionaryGetValue(
          environment_dict, CFSTR("CLAWBROWSER_DEFAULT_FINGERPRINT_ID"));
      if (fingerprint_ref &&
          CFGetTypeID(fingerprint_ref) == CFStringGetTypeID()) {
        default_fingerprint =
            CopyCfStringToUtf8(static_cast<CFStringRef>(fingerprint_ref));
      }
    }
  }

  CFRelease(plist);
  return default_fingerprint;
}
#endif

std::optional<std::string> ResolveDefaultFingerprintForAuthRelaunch(
    const base::CommandLine& current_command_line) {
  if (std::optional<std::string> default_fingerprint =
          ResolveDefaultFingerprintFromEnvironment();
      default_fingerprint.has_value()) {
    return default_fingerprint;
  }

  ProfileManager profile_manager(GetConfigDir());
  if (std::optional<std::string> cached_profile =
          profile_manager.FindBestCachedProfileId();
      cached_profile.has_value()) {
    return cached_profile;
  }

#if BUILDFLAG(IS_MAC)
  if (std::optional<std::string> default_fingerprint =
          ResolveDefaultFingerprintFromBundle(current_command_line.GetProgram());
      default_fingerprint.has_value()) {
    return default_fingerprint;
  }
#else
  (void)current_command_line;
#endif

  return std::string(kDefaultFingerprintId);
}

void EnsureFingerprintModeForAuthRelaunch(
    const base::CommandLine& current_command_line,
    base::CommandLine* relaunch_command_line) {
  if (!relaunch_command_line->HasSwitch("fingerprint")) {
    if (std::optional<std::string> default_fingerprint =
            ResolveDefaultFingerprintForAuthRelaunch(current_command_line);
        default_fingerprint.has_value()) {
      relaunch_command_line->AppendSwitchASCII("fingerprint",
                                               *default_fingerprint);
    }
  }

  if (relaunch_command_line->HasSwitch("fingerprint") &&
      !relaunch_command_line->HasSwitch("regenerate")) {
    relaunch_command_line->AppendSwitch("regenerate");
  }
}

bool LaunchAuthRelaunchProcess(const base::CommandLine& relaunch_command_line) {
#if BUILDFLAG(IS_POSIX)
  base::CommandLine shell(
      base::FilePath::FromUTF8Unsafe("/bin/sh"));
  shell.AppendArg("-c");
  shell.AppendArg("sleep 1; exec \"$0\" \"$@\"");
  for (const auto& arg : relaunch_command_line.argv()) {
    shell.AppendArgNative(arg);
  }

  base::LaunchOptions options;
  options.new_process_group = true;
  return base::LaunchProcess(shell, options).IsValid();
#else
  return base::LaunchProcess(relaunch_command_line, base::LaunchOptions())
      .IsValid();
#endif
}

}  // namespace

std::string_view GetAuthDashboardUrl() {
#if defined(CLAWBROWSER_DEFAULT_DASHBOARD_URL)
  return CLAWBROWSER_DEFAULT_DASHBOARD_URL;
#else
  return "https://app.qa.clawbrowser.ai";
#endif
}

std::string_view GetAuthRestartMessage() {
  return "API key saved. Restarting Clawbrowser...";
}

base::CommandLine BuildAuthRelaunchCommandLine(
    const base::CommandLine& current_command_line) {
  base::CommandLine relaunch_command_line(current_command_line.GetProgram());
  for (const auto& [switch_name, switch_value] :
       current_command_line.GetSwitches()) {
    if (!ShouldCopySwitchForAuthRelaunch(switch_name)) {
      continue;
    }
    relaunch_command_line.AppendSwitchNative(switch_name, switch_value);
  }
  for (const auto& arg : current_command_line.GetArgs()) {
    if (!ShouldCopyArgForAuthRelaunch(arg)) {
      continue;
    }
    relaunch_command_line.AppendArgNative(arg);
  }
  EnsureFingerprintModeForAuthRelaunch(current_command_line,
                                       &relaunch_command_line);
  return relaunch_command_line;
}

bool IsValidApiKeyInput(std::string_view input) {
  return !base::TrimWhitespaceASCII(std::string(input), base::TRIM_ALL).empty();
}

AuthPageUI::AuthPageUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kAuthHost);
  source->SetSupportedScheme("clawbrowser");
  SetupDataSource(source);
  web_ui->AddRequestableScheme("clawbrowser");

  web_ui->RegisterMessageCallback(
      "openDashboard",
      base::BindRepeating(&AuthPageUI::HandleOpenDashboard,
                          base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "saveApiKey",
      base::BindRepeating(&AuthPageUI::HandleSaveApiKey,
                          base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "restartAfterSave",
      base::BindRepeating(&AuthPageUI::HandleRestartAfterSave,
                          base::Unretained(this)));
}

AuthPageUI::~AuthPageUI() = default;

void AuthPageUI::SetupDataSource(content::WebUIDataSource* source) {
  source->AddResourcePath("auth.html", IDR_CLAWBROWSER_AUTH_HTML);
  source->AddResourcePath("auth.js", IDR_CLAWBROWSER_AUTH_JS);
  source->AddResourcePath("auth.css", IDR_CLAWBROWSER_AUTH_CSS);
  source->AddResourcePath("side-bite.svg", IDR_CLAWBROWSER_SIDE_BITE_SVG);
  source->SetDefaultResource(IDR_CLAWBROWSER_AUTH_HTML);

  source->AddString("dashboard_url", std::string(GetAuthDashboardUrl()));
}

void AuthPageUI::HandleOpenDashboard(const base::ListValue& args) {
  (void)args;
  content::WebContents* web_contents = web_ui()->GetWebContents();
  if (!web_contents) {
    return;
  }

  const GURL dashboard_url{std::string(GetAuthDashboardUrl())};
  if (!dashboard_url.is_valid()) {
    return;
  }

  content::OpenURLParams params(
      dashboard_url, content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      /*is_renderer_initiated=*/false,
      /*started_from_context_menu=*/false);
  web_contents->OpenURL(
      params, /*navigation_handle_callback=*/{});
}

void AuthPageUI::HandleSaveApiKey(const base::ListValue& args) {
  auto send_result = [this](base::DictValue result) {
    web_ui()->CallJavascriptFunctionUnsafe("onApiKeySaveResult",
                                           base::Value(std::move(result)));
  };

  std::string api_key;
  if (!args.empty() && args[0].is_string()) {
    api_key =
        base::TrimWhitespaceASCII(args[0].GetString(), base::TRIM_ALL);
  }

  if (!IsValidApiKeyInput(api_key)) {
    send_result(
        MakeSaveResult(false, "Enter the API key you copied from the dashboard."));
    return;
  }

  ProfileManager profile_manager(GetConfigDir());
  auto save_result = profile_manager.SaveApiKey(api_key);
  if (!save_result.has_value()) {
    send_result(MakeSaveResult(false, save_result.error()));
    return;
  }

  send_result(MakeSaveResult(
      true,
      std::string(GetAuthRestartMessage()),
      true));
}

void AuthPageUI::HandleRestartAfterSave(const base::ListValue& args) {
  (void)args;
  auto send_result = [this](base::DictValue result) {
    web_ui()->CallJavascriptFunctionUnsafe("onRestartAfterSaveResult",
                                           base::Value(std::move(result)));
  };

  base::CommandLine relaunch_command_line =
      BuildAuthRelaunchCommandLine(*base::CommandLine::ForCurrentProcess());
  if (!LaunchAuthRelaunchProcess(relaunch_command_line)) {
    send_result(MakeRestartFailureResult(
        "API key saved, but automatic restart failed. Quit and reopen Clawbrowser once."));
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce([]() { chrome::ExitIgnoreUnloadHandlers(); }));
}

}  // namespace clawbrowser
