#include "clawbrowser/startup.h"

#include <utility>

#include "base/environment.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "clawbrowser/cli/api_client.h"
#include "clawbrowser/cli/args.h"
#include "clawbrowser/cli/profile_manager.h"
#include "clawbrowser/fingerprint_accessor.h"
#include "clawbrowser/fingerprint_loader.h"
#include "clawbrowser/logging.h"
#include "clawbrowser/proxy/proxy_config.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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

bool ValidateFingerprintId(const std::string& fp_id,
                           const ClawArgs& args,
                           StartupResult* result) {
  if (!fp_id.empty() && base::StartsWith(fp_id, "fp_")) {
    return true;
  }

  PrintError(args, "invalid_fingerprint_id",
             "invalid fingerprint ID: must start with 'fp_', got: " + fp_id);
  result->should_exit = true;
  result->exit_code = 1;
  return false;
}

inline constexpr char kUserAgentMajorVersionSwitch[] =
    "clawbrowser-ua-major-version";
inline constexpr char kUserAgentFullVersionSwitch[] =
    "clawbrowser-ua-full-version";
inline constexpr char kUserAgentPlatformSwitch[] =
    "clawbrowser-ua-platform";

std::optional<std::string> ExtractChromeFullVersion(
    const std::string& user_agent) {
  constexpr char kChromeToken[] = "Chrome/";
  const size_t token = user_agent.find(kChromeToken);
  if (token == std::string::npos) {
    return std::nullopt;
  }

  const size_t start = token + std::char_traits<char>::length(kChromeToken);
  const size_t end = user_agent.find_first_of(" )", start);
  const std::string version = user_agent.substr(start, end - start);
  if (version.empty()) {
    return std::nullopt;
  }

  return version;
}

std::optional<std::string> ExtractChromeMajorVersion(
    const std::string& user_agent) {
  std::optional<std::string> full_version =
      ExtractChromeFullVersion(user_agent);
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
    request->browser = "chrome";
  }
  if (request->country.empty() && !args.has_location_overrides()) {
    request->country = "US";
  }
}

}  // namespace

base::expected<StartupResult, std::string> ConfigureEarlyStartup(
    base::CommandLine* command_line) {
  ClawArgs args = ClawArgs::Parse(*command_line);
  SetVerbose(args.verbose());

  StartupResult result;
  ProfileManager profile_manager(GetConfigDir());

  if (args.list()) {
    return base::ok(HandleListProfiles(args, &profile_manager));
  }

  if (args.is_vanilla()) {
    command_line->AppendSwitchPath(
        "user-data-dir", profile_manager.GetVanillaUserDataDir());
    return base::ok(std::move(result));
  }

  const std::string& fp_id = args.fingerprint_id();
  if (!ValidateFingerprintId(fp_id, args, &result)) {
    return base::ok(std::move(result));
  }

  command_line->AppendSwitchPath(
      "user-data-dir", profile_manager.GetUserDataDir(fp_id));
  return base::ok(std::move(result));
}

base::expected<StartupResult, std::string> RunStartup(
    base::CommandLine* command_line,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  ClawArgs args = ClawArgs::Parse(*command_line);
  SetVerbose(args.verbose());
  StartupResult result;

  CLAW_VLOG() << "starting with args: fingerprint="
              << args.fingerprint_id();

  // Resolve config directory
  ProfileManager profile_manager(GetConfigDir());

  // Handle --list
  if (args.list()) {
    return base::ok(HandleListProfiles(args, &profile_manager));
  }

  // Vanilla mode
  if (args.is_vanilla()) {
    command_line->AppendSwitchPath(
        "user-data-dir", profile_manager.GetVanillaUserDataDir());
    return base::ok(std::move(result));
  }

  // Fingerprint mode
  const std::string& fp_id = args.fingerprint_id();
  if (!ValidateFingerprintId(fp_id, args, &result)) {
    return base::ok(std::move(result));
  }

  bool needs_fetch = args.regenerate() ||
                     !profile_manager.HasCachedProfile(fp_id);

  if (needs_fetch) {
    // Resolve API key
    auto api_key = profile_manager.ResolveApiKey();
    if (!api_key) {
      PrintError(args, "no_api_key",
                 "API key not found. Set CLAWBROWSER_API_KEY or add "
                 "api_key to config.json");
      result.should_exit = true;
      result.exit_code = 1;
      return base::ok(std::move(result));
    }

    std::string base_url = profile_manager.ResolveBaseUrl();
    ApiClient client(base_url, *api_key, url_loader_factory);

    // Build request params (replay from cached profile if --regenerate)
    GenerateRequest params;
    params.platform = "macos";
    params.browser = "chrome";
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
      std::string msg;
      if (err.http_status == 0)
        msg = "cannot reach API at " + base_url + ": " + err.message;
      else if (err.http_status == 401)
        msg = "invalid API key";
      else if (err.http_status == 429)
        msg = "rate limited, try again later";
      else
        msg = "API server error: " + err.message;
      PrintError(args, err.code, msg);
      result.should_exit = true;
      result.exit_code = 1;
      return base::ok(std::move(result));
    }

    // Save profile envelope
    ProfileEnvelope envelope;
    envelope.schema_version = ProfileEnvelope::kCurrentSchemaVersion;
    envelope.created_at = base::TimeFormatAsIso8601(base::Time::Now());
    envelope.request = params;
    envelope.response = std::move(*api_result);

    auto save_result = profile_manager.SaveProfile(fp_id, envelope);
    if (!save_result.has_value()) {
      PrintError(args, "write_error", save_result.error());
      result.should_exit = true;
      result.exit_code = 1;
      return base::ok(std::move(result));
    }
  }

  // Load fingerprint into accessor
  base::FilePath fp_path = profile_manager.GetFingerprintPath(fp_id);
  auto load_result = LoadFingerprint(fp_path);
  if (!load_result.has_value()) {
    PrintError(args, "load_error", load_result.error());
    result.should_exit = true;
    result.exit_code = 1;
    return base::ok(std::move(result));
  }

  // Set command-line flags for Chromium
  command_line->AppendSwitchPath("clawbrowser-fp-path", fp_path);
  command_line->AppendSwitchPath(
      "user-data-dir", profile_manager.GetUserDataDir(fp_id));
#if BUILDFLAG(IS_MAC)
  auto child_payload = BuildChildFingerprintPayload(fp_path);
  if (!child_payload.has_value()) {
    PrintError(args, "load_error", child_payload.error());
    result.should_exit = true;
    result.exit_code = 1;
    return base::ok(std::move(result));
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
        ExtractChromeMajorVersion(fp->user_agent);
    std::optional<std::string> full_version =
        ExtractChromeFullVersion(fp->user_agent);
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
