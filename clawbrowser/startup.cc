#include "clawbrowser/startup.h"

#include <string_view>
#include <utility>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "clawbrowser/cli/api_client.h"
#include "clawbrowser/cli/args.h"
#include "clawbrowser/defaults.h"
#include "clawbrowser/cli/profile_manager.h"
#include "clawbrowser/fingerprint_accessor.h"
#include "clawbrowser/fingerprint_loader.h"
#include "clawbrowser/logging.h"
#include "clawbrowser/paths.h"
#include "clawbrowser/proxy/proxy_config.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace clawbrowser {

namespace {

void PrintError(const ClawArgs& args, const std::string& code,
                const std::string& message) {
  // Errors always print to stderr regardless of --verbose
  fprintf(stderr, "[clawbrowser] error: %s\n", message.c_str());
  if (args.json_output()) {
    base::DictValue error;
    error.Set("error", code);
    error.Set("message", message);
    std::string json;
    base::JSONWriter::Write(base::Value(std::move(error)), &json);
    fprintf(stdout, "%s\n", json.c_str());
  }
}

StartupResult HandleListProfiles(const ClawArgs& args,
                                 ProfileManager* profile_manager) {
  auto profiles = profile_manager->ListProfiles();
  if (args.json_output()) {
    base::ListValue list;
    for (const auto& p : profiles) {
      base::DictValue item;
      item.Set("id", p.id);
      item.Set("created_at", p.created_at);
      item.Set("country", p.country);
      list.Append(std::move(item));
    }
    std::string json;
    base::JSONWriter::WriteWithOptions(
        base::Value(std::move(list)),
        base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
    fprintf(stdout, "%s\n", json.c_str());
  } else {
    for (const auto& p : profiles) {
      fprintf(stdout, "%s  %s  %s\n",
              p.id.c_str(), p.created_at.c_str(), p.country.c_str());
    }
  }

  StartupResult result;
  result.should_exit = true;
  return result;
}

inline constexpr char kUserAgentMajorVersionSwitch[] =
    "clawbrowser-ua-major-version";
inline constexpr char kUserAgentFullVersionSwitch[] =
    "clawbrowser-ua-full-version";
inline constexpr char kUserAgentPlatformSwitch[] =
    "clawbrowser-ua-platform";

std::optional<std::string> ExtractClawbrowserFullVersion(
    const std::string& user_agent) {
  constexpr char kClawbrowserToken[] = "Clawbrowser/";
  const size_t token = user_agent.find(kClawbrowserToken);
  if (token == std::string::npos) {
    return std::nullopt;
  }

  const size_t start =
      token + std::char_traits<char>::length(kClawbrowserToken);
  const size_t end = user_agent.find_first_of(" )", start);
  const std::string version = user_agent.substr(start, end - start);
  if (version.empty()) {
    return std::nullopt;
  }

  return version;
}

std::optional<std::string> ExtractClawbrowserMajorVersion(
    const std::string& user_agent) {
  std::optional<std::string> full_version =
      ExtractClawbrowserFullVersion(user_agent);
  if (!full_version.has_value()) {
    return std::nullopt;
  }

  const size_t dot = full_version->find('.');
  return full_version->substr(0, dot);
}

std::string UserAgentMetadataPlatform(const std::string& platform) {
  if (platform == "MacIntel") {
    return "macOS";
  }
  return platform;
}

bool HasStartupUrl(const base::CommandLine* command_line,
                   base::CommandLine::StringViewType url) {
  for (const auto& arg : command_line->GetArgs()) {
    if (arg == url) {
      return true;
    }
  }
  return false;
}

void ApplyMacAutomationSwitches(base::CommandLine* command_line) {
#if BUILDFLAG(IS_MAC)
  if (!command_line->HasSwitch("use-mock-keychain")) {
    command_line->AppendSwitch("use-mock-keychain");
  }
#endif
}

std::string ResolveImplicitFingerprintId(ProfileManager* profile_manager) {
  auto env = base::Environment::Create();
  if (std::optional<std::string> default_fingerprint =
          env->GetVar("CLAWBROWSER_DEFAULT_FINGERPRINT_ID");
      default_fingerprint.has_value() && !default_fingerprint->empty()) {
    return *default_fingerprint;
  }

  if (std::optional<std::string> cached_profile =
          profile_manager->FindBestCachedProfileId();
      cached_profile.has_value() && !cached_profile->empty()) {
    return *cached_profile;
  }

  return kDefaultFingerprintId;
}

void ApplyDefaultFingerprintSwitch(base::CommandLine* command_line,
                                   ProfileManager* profile_manager) {
  if (command_line->HasSwitch("fingerprint")) {
    return;
  }

  command_line->AppendSwitchASCII(
      "fingerprint", ResolveImplicitFingerprintId(profile_manager));
}

void ConfigureAuthStartup(base::CommandLine* command_line,
                          ProfileManager* profile_manager);
void ConfigureVanillaUserDataDir(base::CommandLine* command_line,
                                 ProfileManager* profile_manager);

void ConfigureProfileStartupCommandLine(const ClawArgs& args,
                                        base::CommandLine* command_line,
                                        ProfileManager* profile_manager) {
  if (args.list()) {
    return;
  }

  const bool has_api_key = profile_manager->ResolveApiKey().has_value();

  if (args.is_vanilla()) {
    if (!has_api_key) {
      ConfigureAuthStartup(command_line, profile_manager);
    } else {
      ConfigureVanillaUserDataDir(command_line, profile_manager);
    }
    return;
  }

  const std::string& fp_id = args.fingerprint_id();
  if (!has_api_key) {
    ConfigureAuthStartup(command_line, profile_manager);
    return;
  }

  command_line->AppendSwitchPath("user-data-dir",
                                 profile_manager->GetUserDataDir(fp_id));
}

base::FilePath GetAuthUserDataDir(ProfileManager* profile_manager) {
  return profile_manager->GetVanillaUserDataDir().DirName().AppendASCII("Auth");
}

bool ShouldKeepVanillaStartupArg(
    const base::CommandLine::StringType& arg) {
  return arg != FILE_PATH_LITERAL("clawbrowser://verify/");
}

bool ShouldKeepAuthStartupArg(
    const base::CommandLine::StringType& arg) {
  return arg != FILE_PATH_LITERAL("clawbrowser://auth/") &&
         arg != FILE_PATH_LITERAL("clawbrowser://verify/");
}

bool ShouldKeepAuthStartupSwitch(std::string_view switch_name) {
  return switch_name != "restore-last-session" &&
         switch_name != "user-data-dir" &&
         switch_name != "no-startup-window";
}

void AppendAuthPage(base::CommandLine* command_line);

void StripVerifyPage(base::CommandLine* command_line) {
  if (!HasStartupUrl(command_line,
                     FILE_PATH_LITERAL("clawbrowser://verify/"))) {
    return;
  }

  base::CommandLine filtered(command_line->GetProgram());
  for (const auto& [switch_name, switch_value] : command_line->GetSwitches()) {
    filtered.AppendSwitchNative(switch_name, switch_value);
  }
  for (const auto& arg : command_line->GetArgs()) {
    if (!ShouldKeepVanillaStartupArg(arg)) {
      continue;
    }
    filtered.AppendArgNative(arg);
  }
  *command_line = filtered;
}

void ConfigureAuthStartup(base::CommandLine* command_line,
                          ProfileManager* profile_manager) {
  base::CommandLine filtered(command_line->GetProgram());
  for (const auto& [switch_name, switch_value] : command_line->GetSwitches()) {
    if (!ShouldKeepAuthStartupSwitch(switch_name)) {
      continue;
    }
    filtered.AppendSwitchNative(switch_name, switch_value);
  }
  for (const auto& arg : command_line->GetArgs()) {
    if (!ShouldKeepAuthStartupArg(arg)) {
      continue;
    }
    filtered.AppendArgNative(arg);
  }
  filtered.AppendSwitchPath("user-data-dir", GetAuthUserDataDir(profile_manager));
  AppendAuthPage(&filtered);
  *command_line = filtered;
}

void ConfigureVanillaUserDataDir(base::CommandLine* command_line,
                                 ProfileManager* profile_manager) {
  command_line->AppendSwitchPath(
      "user-data-dir", profile_manager->GetVanillaUserDataDir());
}

void AppendAuthPage(base::CommandLine* command_line) {
  if (HasStartupUrl(command_line, FILE_PATH_LITERAL("clawbrowser://auth/"))) {
    return;
  }
  command_line->AppendArg("clawbrowser://auth/");
}

StartupResult FallbackToVanillaBrowser(base::CommandLine* command_line,
                                       ProfileManager* profile_manager,
                                       bool append_auth_page) {
  FingerprintAccessor::Reset();
  if (append_auth_page) {
    ConfigureAuthStartup(command_line, profile_manager);
    return StartupResult();
  }
  StripVerifyPage(command_line);
  ConfigureVanillaUserDataDir(command_line, profile_manager);
  return StartupResult();
}

void PrintWarning(const std::string& message) {
  fprintf(stderr, "[clawbrowser] warning: %s\n", message.c_str());
}

constexpr char kBackendBrowserName[] = "chrome";

void ApplyGenerateRequestOverrides(const ClawArgs& args,
                                   GenerateRequest* request) {
  if (args.has_location_overrides()) {
    request->country.clear();
    request->city.reset();
    request->connection_type.reset();
  }

  if (args.has_country_override()) {
    request->country = args.country();
  }
  if (args.has_city_override()) {
    request->city = args.city();
  }
  if (args.has_connection_type_override()) {
    request->connection_type = args.connection_type();
  }

  if (request->platform.empty()) {
    request->platform = "macos";
  }
  if (request->browser.empty()) {
    request->browser = kBackendBrowserName;
  }
  if (request->country.empty() && !args.has_location_overrides()) {
    request->country = "US";
  }
}

}  // namespace

base::expected<std::optional<int>, std::string> HandleBasicStartupComplete(
    const base::CommandLine& command_line) {
  ClawArgs args = ClawArgs::Parse(command_line);
  SetVerbose(args.verbose());

  if (!args.list()) {
    return base::ok(std::nullopt);
  }

  ProfileManager profile_manager(GetClawbrowserConfigDir());
  HandleListProfiles(args, &profile_manager);
  return base::ok(0);
}

void ConfigureCommandLineBeforeUserDataDir(base::CommandLine* command_line) {
  ApplyMacAutomationSwitches(command_line);

  ProfileManager profile_manager(GetClawbrowserConfigDir());
  ApplyDefaultFingerprintSwitch(command_line, &profile_manager);

  ClawArgs args = ClawArgs::Parse(*command_line);
  SetVerbose(args.verbose());
  ConfigureProfileStartupCommandLine(args, command_line, &profile_manager);
}

base::expected<StartupResult, std::string> ConfigureEarlyStartup(
    base::CommandLine* command_line) {
  ConfigureCommandLineBeforeUserDataDir(command_line);
  auto basic_startup_result = HandleBasicStartupComplete(*command_line);
  if (!basic_startup_result.has_value()) {
    return base::unexpected(basic_startup_result.error());
  }

  StartupResult result;
  if (basic_startup_result->has_value()) {
    result.should_exit = true;
    result.exit_code = basic_startup_result->value();
    return base::ok(std::move(result));
  }

  return base::ok(std::move(result));
}

base::expected<StartupResult, std::string> RunStartup(
    base::CommandLine* command_line,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  ApplyMacAutomationSwitches(command_line);
  ProfileManager profile_manager(GetClawbrowserConfigDir());
  ApplyDefaultFingerprintSwitch(command_line, &profile_manager);
  StartupResult result;

  auto basic_startup_result = HandleBasicStartupComplete(*command_line);
  if (!basic_startup_result.has_value()) {
    return base::unexpected(basic_startup_result.error());
  }
  if (basic_startup_result->has_value()) {
    result.should_exit = true;
    result.exit_code = basic_startup_result->value();
    return base::ok(std::move(result));
  }

  ClawArgs args = ClawArgs::Parse(*command_line);
  SetVerbose(args.verbose());

  CLAW_VLOG() << "starting with args: fingerprint="
              << args.fingerprint_id();

  // Vanilla mode
  if (args.is_vanilla()) {
    if (!profile_manager.ResolveApiKey().has_value()) {
      ConfigureAuthStartup(command_line, &profile_manager);
    } else {
      ConfigureVanillaUserDataDir(command_line, &profile_manager);
    }
    return base::ok(std::move(result));
  }

  // Fingerprint mode
  const std::string& fp_id = args.fingerprint_id();
  const bool needs_fetch = args.regenerate() ||
                           !profile_manager.HasCachedProfile(fp_id);
  std::optional<std::string> api_key = profile_manager.ResolveApiKey();
  if (!api_key.has_value()) {
    ConfigureAuthStartup(command_line, &profile_manager);
    return base::ok(std::move(result));
  }

  if (needs_fetch) {
    std::optional<std::string> base_url = profile_manager.ResolveBaseUrl();
    if (!base_url.has_value()) {
      PrintError(args, "no_api_base_url",
                 "API base URL not found. Set CLAWBROWSER_API_BASE_URL or add "
                 "api_base_url to config.json");
      result.should_exit = true;
      result.exit_code = 1;
      return base::ok(std::move(result));
    }

    ApiClient client(*base_url, *api_key, url_loader_factory);

    // Build request params (replay from cached profile if --regenerate)
    GenerateRequest params;
    params.platform = "macos";
    params.browser = kBackendBrowserName;
    params.country = "US";
    if (args.regenerate() && profile_manager.HasCachedProfile(fp_id)) {
      auto cached = profile_manager.ReadProfile(fp_id);
      if (cached.has_value()) {
        params = cached->request;
      }
    }
    ApplyGenerateRequestOverrides(args, &params);

    // Synchronous API call (blocking — acceptable for pre-launch)
    base::RunLoop run_loop;
    base::expected<GenerateResponse, ApiError> api_result;
    client.GenerateFingerprint(params,
        base::BindOnce([](base::RunLoop* loop,
                          base::expected<GenerateResponse, ApiError>* out,
                          base::expected<GenerateResponse, ApiError> r) {
          *out = std::move(r);
          loop->Quit();
        }, &run_loop, &api_result));
    run_loop.Run();

    if (!api_result.has_value()) {
      const auto& err = api_result.error();
      const bool invalid_api_key =
          err.http_status == 401 || err.http_status == 403;
      std::string msg;
      if (err.http_status == 0)
        msg = "cannot reach API at " + *base_url + ": " + err.message;
      else if (invalid_api_key)
        msg = "invalid API key";
      else if (err.http_status == 429)
        msg = "rate limited, try again later";
      else
        msg = "API server error: " + err.message;
      PrintWarning(msg + (invalid_api_key ? "; opening auth page"
                                          : "; falling back to vanilla browser"));
      return base::ok(FallbackToVanillaBrowser(
          command_line, &profile_manager,
          /*append_auth_page=*/invalid_api_key));
    }

    // Save profile envelope
    ProfileEnvelope envelope;
    envelope.schema_version = ProfileEnvelope::kCurrentSchemaVersion;
    envelope.created_at = base::TimeFormatAsIso8601(base::Time::Now());
    envelope.request = params;
    envelope.response = std::move(*api_result);

    auto save_result = profile_manager.SaveProfile(fp_id, envelope);
    if (!save_result.has_value()) {
      PrintWarning("failed to save fingerprint profile: " + save_result.error() +
                   "; falling back to vanilla browser");
      return base::ok(FallbackToVanillaBrowser(
          command_line, &profile_manager, /*append_auth_page=*/false));
    }
  }

  // Load fingerprint into accessor
  base::FilePath fp_path = profile_manager.GetFingerprintPath(fp_id);
  auto load_result = LoadFingerprint(fp_path);
  if (!load_result.has_value()) {
    PrintWarning("failed to load fingerprint profile: " + load_result.error() +
                 "; falling back to vanilla browser");
    return base::ok(FallbackToVanillaBrowser(
        command_line, &profile_manager, /*append_auth_page=*/false));
  }

  // Set command-line flags for Clawbrowser
  command_line->AppendSwitchPath("clawbrowser-fp-path", fp_path);
  command_line->AppendSwitchPath(
      "user-data-dir", profile_manager.GetUserDataDir(fp_id));
#if BUILDFLAG(IS_MAC)
  auto child_payload = BuildChildFingerprintPayload(fp_path);
  if (!child_payload.has_value()) {
    PrintWarning("failed to prepare child fingerprint payload: " +
                 child_payload.error() + "; falling back to vanilla browser");
    return base::ok(FallbackToVanillaBrowser(
        command_line, &profile_manager, /*append_auth_page=*/false));
  }
  command_line->AppendSwitchASCII(kFingerprintChildDataSwitch, *child_payload);
#endif

  // Configure proxy flags
  const auto* proxy = FingerprintAccessor::GetProxy();
  if (proxy) {
    auto proxy_flags = GetProxyCommandLineFlags(*proxy);
    for (const auto& flag : proxy_flags) {
      // Parse --key=value from flag string
      size_t eq = flag.find('=');
      if (eq != std::string::npos) {
        command_line->AppendSwitchASCII(
            flag.substr(2, eq - 2),  // strip leading --
            flag.substr(eq + 1));
      }
    }
  }

  // Set language flags from fingerprint (Accept-Language header alignment)
  const auto* fp = FingerprintAccessor::Get();
  if (fp && !fp->user_agent.empty()) {
    command_line->AppendSwitchASCII("user-agent", fp->user_agent);
    std::optional<std::string> major_version =
        ExtractClawbrowserMajorVersion(fp->user_agent);
    std::optional<std::string> full_version =
        ExtractClawbrowserFullVersion(fp->user_agent);
    if (major_version.has_value() && full_version.has_value()) {
      command_line->AppendSwitchASCII(kUserAgentMajorVersionSwitch,
                                      *major_version);
      command_line->AppendSwitchASCII(kUserAgentFullVersionSwitch,
                                      *full_version);
      command_line->AppendSwitchASCII(kUserAgentPlatformSwitch,
                                      UserAgentMetadataPlatform(fp->platform));
    }
  }
  if (fp && !fp->language.empty()) {
    // --lang sets the UI language
    command_line->AppendSwitchASCII("lang", fp->language[0]);
    // --accept-lang sets the Accept-Language HTTP header
    std::string accept_lang;
    for (size_t i = 0; i < fp->language.size(); ++i) {
      if (i > 0) accept_lang += ",";
      accept_lang += fp->language[i];
    }
    command_line->AppendSwitchASCII("accept-lang", accept_lang);
  }

  // Navigate to verify page on startup (unless --skip-verify)
  if (!args.skip_verify()) {
    command_line->AppendArg("clawbrowser://verify/");
  }

  return base::ok(std::move(result));
}

}  // namespace clawbrowser
