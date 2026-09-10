#include "clawbrowser/startup.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "clawbrowser/cli/api_client.h"
#include "clawbrowser/cli/args.h"
#include "clawbrowser/defaults.h"
#include "clawbrowser/cli/profile_manager.h"
#include "clawbrowser/fingerprint_accessor.h"
#include "clawbrowser/fingerprint_coherence.h"
#include "clawbrowser/fingerprint_loader.h"
#include "clawbrowser/logging.h"
#include "clawbrowser/paths.h"
#include "clawbrowser/proxy/proxy_config.h"
#include "clawbrowser/proxy/socks5_auth_proxy_bridge.h"
#include "components/version_info/version_info.h"
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

bool IsSwiftShaderWebGLBackend(const base::CommandLine& command_line) {
  const std::string use_gl = command_line.GetSwitchValueASCII("use-gl");
  const std::string use_angle = command_line.GetSwitchValueASCII("use-angle");
  return base::StartsWith(use_gl, "swiftshader",
                          base::CompareCase::INSENSITIVE_ASCII) ||
         base::StartsWith(use_angle, "swiftshader",
                          base::CompareCase::INSENSITIVE_ASCII);
}

// A renderer string can only be spoofed coherently when all of the WebGL
// capabilities behind it belong to the same adapter. Fingerprint profiles
// therefore use ANGLE's bundled SwiftShader backend: it hides the host GPU at
// the source and gives every platform a real, internally consistent software
// adapter. Vanilla mode remains available when native GPU acceleration is
// preferred over fingerprint isolation.
void ApplyFingerprintWebGLIsolation(const ClawArgs& args,
                                    base::CommandLine* command_line) {
  if (args.list() || args.is_vanilla()) {
    return;
  }

  command_line->RemoveSwitch("use-gl");
  command_line->RemoveSwitch("use-angle");
  command_line->RemoveSwitch("use-webgpu-adapter");
  command_line->AppendSwitchASCII("use-gl", "angle");
  command_line->AppendSwitchASCII("use-angle", "swiftshader");
  // WebGPU selects a Dawn adapter independently from ANGLE. Pin it to the
  // bundled fallback adapter as well; otherwise WebGL reports SwiftShader
  // while navigator.gpu still exposes the host Metal/D3D/Vulkan device.
  command_line->AppendSwitchASCII("use-webgpu-adapter", "swiftshader");

  // Renderer/GPU children reconstruct the surface policy from the profile and
  // raw command-line switches. Force the resolved native WebGL policy onto
  // their command lines too, including while an older backend or cached
  // profile still says "override". Otherwise those children would put the
  // stale vendor/renderer overlay back on top of SwiftShader's real limits.
  if (!command_line->HasSwitch(kDisableWebGLSpoofingSwitch)) {
    command_line->AppendSwitch(kDisableWebGLSpoofingSwitch);
  }
}

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

  ApplyFingerprintWebGLIsolation(args, command_line);
  command_line->AppendSwitchPath("user-data-dir",
                                 profile_manager->GetUserDataDir(fp_id));
}

base::FilePath GetAuthUserDataDir(ProfileManager* profile_manager) {
  return profile_manager->GetVanillaUserDataDir().DirName().AppendASCII("Auth");
}

bool ShouldKeepAuthStartupSwitch(std::string_view switch_name) {
  return switch_name != "restore-last-session" &&
         switch_name != "user-data-dir" &&
         switch_name != "no-startup-window";
}

void AppendAuthPage(base::CommandLine* command_line);

void ConfigureAuthStartup(base::CommandLine* command_line,
                          ProfileManager* profile_manager) {
  base::CommandLine filtered(command_line->GetProgram());
  for (const auto& [switch_name, switch_value] : command_line->GetSwitches()) {
    if (!ShouldKeepAuthStartupSwitch(switch_name)) {
      continue;
    }
    filtered.AppendSwitchNative(switch_name, switch_value);
  }
  // Auth runs without a managed fingerprint. Never carry startup URL/file
  // arguments into that profile: doing so could load the requested target with
  // native browser surfaces before authentication completes.
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

std::unique_ptr<Socks5AuthProxyBridge>& ActiveSocks5AuthProxyBridge() {
  static base::NoDestructor<std::unique_ptr<Socks5AuthProxyBridge>> bridge;
  return *bridge;
}

void StopSocks5AuthProxyBridge() {
  ActiveSocks5AuthProxyBridge().reset();
}

StartupResult FailManagedFingerprintStartup(const ClawArgs& args,
                                            const std::string& code,
                                            const std::string& message) {
  StopSocks5AuthProxyBridge();
  FingerprintAccessor::Reset();
  PrintError(args, code, message);

  StartupResult result;
  result.should_exit = true;
  result.exit_code = 1;
  return result;
}

constexpr char kBackendBrowserName[] = "chrome";

std::string DefaultProfilePlatform() {
#if BUILDFLAG(IS_MAC)
  return "macos";
#elif BUILDFLAG(IS_WIN)
  return "windows";
#elif BUILDFLAG(IS_LINUX)
  return "linux";
#else
  return "macos";
#endif
}

std::string RuntimeOS() {
#if BUILDFLAG(IS_MAC)
  return "macos";
#elif BUILDFLAG(IS_WIN)
  return "windows";
#elif BUILDFLAG(IS_LINUX)
  return "linux";
#else
  return std::string();
#endif
}

std::string RuntimeArch() {
#if defined(ARCH_CPU_ARM64)
  return "arm64";
#elif defined(ARCH_CPU_X86_64)
  return "amd64";
#elif defined(ARCH_CPU_X86)
  return "x86";
#else
  return std::string();
#endif
}

std::optional<std::string> RuntimeGPUHint(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch("disable-gpu")) {
    return "swiftshader";
  }
  if (IsSwiftShaderWebGLBackend(command_line)) {
    return "swiftshader";
  }
  const std::string use_angle = command_line.GetSwitchValueASCII("use-angle");
  // Command-line switches describe the adapter Chromium will actually use and
  // must win over a stale operator hint.  Consult the environment only when no
  // concrete software backend was selected above.
  auto env = base::Environment::Create();
  if (std::optional<std::string> runtime_gpu =
          env->GetVar("CLAWBROWSER_RUNTIME_GPU");
      runtime_gpu.has_value() && !runtime_gpu->empty()) {
    return runtime_gpu;
  }
#if BUILDFLAG(IS_MAC)
  if (use_angle.empty() || use_angle == "metal") {
    return "apple-metal";
  }
#elif BUILDFLAG(IS_WIN)
  if (use_angle.empty() || use_angle == "d3d11") {
    return "direct3d11";
  }
#endif
  return std::nullopt;
}

bool RuntimeHeadless(const base::CommandLine& command_line) {
  return command_line.HasSwitch("headless");
}

bool CachedProfileNeedsPrivacyUpgrade(ProfileManager* profile_manager,
                                      const std::string& profile_id) {
  auto cached = profile_manager->ReadProfile(profile_id);
  if (!cached.has_value()) {
    return true;
  }
  const auto& policy = cached->response.fingerprint.surface_policy;
  return policy.canvas != "override" ||
         policy.fonts != "native_or_allowlist" ||
         !cached->request.runtime_gpu.has_value() ||
         !base::StartsWith(*cached->request.runtime_gpu, "swiftshader",
                           base::CompareCase::INSENSITIVE_ASCII);
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
  if (args.has_proxy_scheme_override()) {
    request->proxy_scheme = args.proxy_scheme();
  }

  if (request->platform.empty()) {
    request->platform = DefaultProfilePlatform();
  }
  if (request->browser.empty()) {
    request->browser = kBackendBrowserName;
  }
  if (request->country.empty() && !args.has_location_overrides()) {
    request->country = "US";
  }
}

void ApplyRuntimeRequestHints(const base::CommandLine& command_line,
                              GenerateRequest* request) {
  // Targeting fields such as country/city are intentionally replayed from a
  // cached request. Runtime facts are not: after a browser/OS update they must
  // always describe the executable doing this launch, or regeneration can
  // immediately create another stale fingerprint.
  request->runtime_browser_version = version_info::GetVersionNumber();
  std::string os = RuntimeOS();
  if (!os.empty()) {
    request->runtime_os = std::move(os);
  } else {
    request->runtime_os.reset();
  }
  // We currently have no portable runtime OS-version probe. An absent hint is
  // safer than replaying a value captured on a different host or OS release.
  request->runtime_os_version.reset();
  std::string arch = RuntimeArch();
  if (!arch.empty()) {
    request->runtime_arch = std::move(arch);
  } else {
    request->runtime_arch.reset();
  }
  // The effective launch backend always wins over a value replayed from the
  // cached request. Otherwise a one-time migration from Apple/D3D to
  // SwiftShader would keep asking the backend for an incompatible renderer.
  request->runtime_gpu = RuntimeGPUHint(command_line);
  request->runtime_headless = RuntimeHeadless(command_line);
}

std::string HeaderOrValue(const RuntimeFingerprint& fp,
                          const std::string& header,
                          const std::string& fallback) {
  for (const auto& [name, value] : fp.headers) {
    if (base::EqualsCaseInsensitiveASCII(name, header) && !value.empty()) {
      return value;
    }
  }
  return fallback;
}

// Chromium's --accept-lang switch feeds the intl.accept_languages pref, which
// net::HttpUtil::ExpandLanguageList() parses as bare language codes; it CHECKs
// that no entry contains ';' or whitespace. Backend fingerprints supply a full
// Accept-Language header value, which normally carries q-values such as
// "en-US,en;q=0.9". Strip the parameters so only the codes are handed over --
// Chromium regenerates the q-values itself when building the actual header.
std::string StripLanguageQualityValues(std::string_view header_value) {
  std::vector<std::string> codes;
  for (std::string_view entry : base::SplitStringPiece(
           header_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    std::string_view code = entry.substr(0, entry.find(';'));
    code = base::TrimWhitespaceASCII(code, base::TRIM_ALL);
    if (!code.empty())
      codes.emplace_back(code);
  }
  return base::JoinString(codes, ",");
}

std::string AcceptLanguageFromFingerprint(const RuntimeFingerprint& fp) {
  for (const auto& [name, value] : fp.headers) {
    if (base::EqualsCaseInsensitiveASCII(name, "Accept-Language") &&
        !value.empty()) {
      return StripLanguageQualityValues(value);
    }
  }

  std::string accept_lang;
  for (size_t i = 0; i < fp.language.size(); ++i) {
    if (i > 0)
      accept_lang += ",";
    accept_lang += fp.language[i];
  }
  return accept_lang;
}

base::expected<std::optional<RuntimeProxyConfig>, std::string>
ResolveDevProxyOverride() {
#if defined(CLAWBROWSER_ENABLE_DEV_PROXY_URL_OVERRIDE) && \
    CLAWBROWSER_ENABLE_DEV_PROXY_URL_OVERRIDE
  auto env = base::Environment::Create();
  std::optional<std::string> proxy_url =
      env->GetVar("CLAWBROWSER_DEV_PROXY_URL");
  if (!proxy_url.has_value() || proxy_url->empty()) {
    return base::ok(std::nullopt);
  }

  auto parsed = ParseProxyUrl(*proxy_url);
  if (!parsed.has_value()) {
    return base::unexpected(parsed.error());
  }
  if (parsed->scheme.value_or("http") != "socks5") {
    return base::unexpected(
        "CLAWBROWSER_DEV_PROXY_URL only supports socks5:// URLs");
  }
  return base::ok(std::optional<RuntimeProxyConfig>(std::move(*parsed)));
#else
  return base::ok(std::nullopt);
#endif
}

void ReplaceRuntimeProxy(RuntimeProxyConfig proxy) {
  const RuntimeFingerprint* fp = FingerprintAccessor::Get();
  if (!fp) {
    return;
  }
  FingerprintAccessor::Set(*fp, std::move(proxy));
}

base::expected<std::optional<RuntimeProxyConfig>, std::string>
ApplyDevProxyOverride() {
  auto proxy_override = ResolveDevProxyOverride();
  if (!proxy_override.has_value()) {
    return base::unexpected(proxy_override.error());
  }
  if (!proxy_override->has_value()) {
    return base::ok(std::nullopt);
  }
  RuntimeProxyConfig proxy = std::move(proxy_override->value());
  ReplaceRuntimeProxy(proxy);
  return base::ok(std::optional<RuntimeProxyConfig>(std::move(proxy)));
}

base::expected<std::optional<ProxyBridgeEndpoint>, std::string>
PrepareProxyBridge(const RuntimeProxyConfig& proxy) {
  StopSocks5AuthProxyBridge();
  if (!ShouldUseSocks5AuthBridge(proxy)) {
    return base::ok(std::nullopt);
  }

  auto bridge = Socks5AuthProxyBridge::Start(proxy);
  if (!bridge.has_value()) {
    return base::unexpected(bridge.error());
  }

  ProxyBridgeEndpoint endpoint = (*bridge)->endpoint();
  ActiveSocks5AuthProxyBridge() = std::move(*bridge);
  return base::ok(std::optional<ProxyBridgeEndpoint>(std::move(endpoint)));
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
    StopSocks5AuthProxyBridge();
    if (!profile_manager.ResolveApiKey().has_value()) {
      ConfigureAuthStartup(command_line, &profile_manager);
    } else {
      ConfigureVanillaUserDataDir(command_line, &profile_manager);
    }
    return base::ok(std::move(result));
  }

  // Fingerprint mode
  const std::string& fp_id = args.fingerprint_id();
  const bool has_cached_profile = profile_manager.HasCachedProfile(fp_id);
  const bool needs_fetch =
      args.regenerate() || !has_cached_profile ||
      (has_cached_profile &&
       CachedProfileNeedsPrivacyUpgrade(&profile_manager, fp_id));
  std::optional<std::string> api_key = profile_manager.ResolveApiKey();
  if (!api_key.has_value()) {
    StopSocks5AuthProxyBridge();
    ConfigureAuthStartup(command_line, &profile_manager);
    return base::ok(std::move(result));
  }

  // ConfigureEarlyStartup normally applied this before Chromium initialized
  // the user-data directory.  Keep RunStartup self-contained as well: unit
  // tests and embedders may call it directly, and the runtime hint sent to the
  // backend must describe the adapter that will really be used.
  ApplyFingerprintWebGLIsolation(args, command_line);

  if (needs_fetch) {
    std::optional<std::string> base_url = profile_manager.ResolveBaseUrl();
    if (!base_url.has_value()) {
      return base::ok(FailManagedFingerprintStartup(
          args, "no_api_base_url",
          "API base URL not found. Set CLAWBROWSER_API_BASE_URL or add "
          "api_base_url to config.json"));
    }

    ApiClient client(*base_url, *api_key, url_loader_factory);

    // Replay cached targeting during explicit regeneration and automatic
    // privacy migrations; only a genuinely new profile uses defaults.
    GenerateRequest params;
    params.platform = DefaultProfilePlatform();
    params.browser = kBackendBrowserName;
    params.country = "US";
    if (has_cached_profile) {
      auto cached = profile_manager.ReadProfile(fp_id);
      if (cached.has_value()) {
        params = cached->request;
      }
    }
    ApplyGenerateRequestOverrides(args, &params);
    ApplyRuntimeRequestHints(*command_line, &params);

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
      std::string code = "fingerprint_api_error";
      if (invalid_api_key) {
        code = "invalid_api_key";
      } else if (err.http_status == 0) {
        code = "fingerprint_api_unavailable";
      } else if (err.http_status == 429) {
        code = "fingerprint_api_rate_limited";
      }
      return base::ok(FailManagedFingerprintStartup(args, code, msg));
    }

    // Save profile envelope
    ProfileEnvelope envelope;
    envelope.schema_version = ProfileEnvelope::kCurrentSchemaVersion;
    envelope.created_at = base::TimeFormatAsIso8601(base::Time::Now());
    envelope.request = params;
    envelope.response = std::move(*api_result);

    auto save_result = profile_manager.SaveProfile(fp_id, envelope);
    if (!save_result.has_value()) {
      return base::ok(FailManagedFingerprintStartup(
          args, "fingerprint_save_failed",
          "failed to save fingerprint profile: " + save_result.error()));
    }
  }

  // Load fingerprint into accessor
  base::FilePath fp_path = profile_manager.GetFingerprintPath(fp_id);
  auto load_result = LoadFingerprint(fp_path);
  if (!load_result.has_value()) {
    return base::ok(FailManagedFingerprintStartup(
        args, "fingerprint_load_failed",
        "failed to load fingerprint profile: " + load_result.error()));
  }
  // The fingerprint's surface_policy is the backend's statement of which
  // surfaces this profile wants spoofed, so it supplies the default. Requiring
  // an extra --enable-*-spoofing switch on top of it meant a profile could ask
  // for canvas/WebGL spoofing and silently not get it -- WebGL in particular
  // would keep handing out the host's real adapter while the profile carried a
  // different renderer string.
  //
  // The switches remain an explicit local override in both directions:
  // --enable-* forces spoofing on for a profile that did not request it, and
  // --disable-* wins over everything.
  const RuntimeFingerprint* loaded = FingerprintAccessor::Get();
  FingerprintAccessor::SetSpoofingPolicy(
      ResolveSurfaceSpoofing(
          args.canvas_spoofing_enabled(), args.canvas_spoofing_suppressed(),
          loaded && loaded->surface_policy.canvas == "override"),
      // SwiftShader already supplies a host-independent vendor, renderer,
      // extension set, limits and pixels. Keep its native renderer string so
      // those surfaces cannot contradict one another, even while rolling out
      // against an older backend response that still requests an override.
      !IsSwiftShaderWebGLBackend(*command_line) &&
          ResolveSurfaceSpoofing(
              args.webgl_spoofing_enabled(),
              args.webgl_spoofing_suppressed(),
              loaded && loaded->surface_policy.webgl == "override"));

  auto dev_proxy_result = ApplyDevProxyOverride();
  if (!dev_proxy_result.has_value()) {
    PrintError(args, "invalid_dev_proxy_url",
               "invalid CLAWBROWSER_DEV_PROXY_URL: " +
                   dev_proxy_result.error());
    result.should_exit = true;
    result.exit_code = 1;
    return base::ok(std::move(result));
  }
  std::optional<RuntimeProxyConfig> dev_proxy_override =
      std::move(*dev_proxy_result);
  const auto* proxy = FingerprintAccessor::GetProxy();
  if (args.require_proxy() && !proxy) {
    return base::ok(FailManagedFingerprintStartup(
        args, "required_proxy_missing",
        "profile launch requires a proxy but the fingerprint response did "
        "not include one"));
  }

  // Set command-line flags for Clawbrowser
  command_line->AppendSwitch(kRequireFingerprintSwitch);
  command_line->AppendSwitchPath(kFingerprintPathSwitch, fp_path);
  command_line->AppendSwitchPath(
      "user-data-dir", profile_manager.GetUserDataDir(fp_id));
#if BUILDFLAG(IS_MAC)
  auto child_payload = dev_proxy_override.has_value()
                           ? BuildChildFingerprintPayload(fp_path,
                                                          *dev_proxy_override)
                           : BuildChildFingerprintPayload(fp_path);
  if (!child_payload.has_value()) {
    return base::ok(FailManagedFingerprintStartup(
        args, "fingerprint_child_payload_failed",
        "failed to prepare child fingerprint payload: " +
            child_payload.error()));
  }
  command_line->AppendSwitchASCII(kFingerprintChildDataSwitch, *child_payload);
#endif

  // Configure proxy flags
  if (proxy) {
    auto bridge_endpoint = PrepareProxyBridge(*proxy);
    if (!bridge_endpoint.has_value()) {
      PrintError(args, "proxy_bridge_failed",
                 "failed to start SOCKS5 auth bridge: " +
                     bridge_endpoint.error());
      result.should_exit = true;
      result.exit_code = 1;
      return base::ok(std::move(result));
    }

    auto proxy_flags = bridge_endpoint->has_value()
                           ? GetProxyCommandLineFlags(*proxy,
                                                      bridge_endpoint->value())
                           : GetProxyCommandLineFlags(*proxy);
    if (proxy_flags.empty()) {
      return base::ok(FailManagedFingerprintStartup(
          args, "invalid_proxy_config",
          "fingerprint response included an incomplete or unsupported proxy"));
    }
    for (const auto& flag : proxy_flags) {
      // Parse --key=value from flag string
      size_t eq = flag.find('=');
      if (eq != std::string::npos) {
        command_line->AppendSwitchASCII(
            flag.substr(2, eq - 2),  // strip leading --
            flag.substr(eq + 1));
      }
    }
  } else {
    StopSocks5AuthProxyBridge();
  }

  // Set language flags from fingerprint (Accept-Language header alignment)
  const auto* fp = FingerprintAccessor::Get();
  if (fp && !fp->user_agent.empty()) {
    command_line->AppendSwitchASCII(
        "user-agent", HeaderOrValue(*fp, "User-Agent", fp->user_agent));
  }
  if (fp && !fp->language.empty()) {
    // --lang sets the UI language
    command_line->AppendSwitchASCII("lang", fp->language[0]);
    // --accept-lang sets the Accept-Language HTTP header
    command_line->AppendSwitchASCII("accept-lang",
                                    AcceptLanguageFromFingerprint(*fp));
  }

  // Keep the real OS window inside the screen we claim to be on.
  // window.outerWidth/outerHeight are reported from the actual window and are
  // not spoofed, so a window wider than the advertised screen.availWidth is a
  // contradiction any detector can check in one line. Sizing the window from
  // the spoofed available area removes the mismatch at the source instead of
  // adding another lie on top of it.
  //
  // Explicit --window-size and --window-position values from the caller always
  // win; those are deliberate, controlled overrides. Otherwise anchor every
  // fingerprinted window at the virtual screen origin, including callers that
  // supply only a custom size.
  if (fp && !command_line->HasSwitch("window-size")) {
    const int avail_width =
        fp->screen.avail_width > 0 ? fp->screen.avail_width : fp->screen.width;
    const int avail_height = fp->screen.avail_height > 0
                                 ? fp->screen.avail_height
                                 : fp->screen.height;
    const WindowSize window =
        DeriveWindowSize(avail_width, avail_height,
                         static_cast<uint64_t>(fp->canvas_seed));
    if (window.width > 0 && window.height > 0) {
      command_line->AppendSwitchASCII(
          "window-size", base::NumberToString(window.width) + "," +
                             base::NumberToString(window.height));
    }
  }
  if (fp && !command_line->HasSwitch("window-position")) {
    command_line->AppendSwitchASCII("window-position", "0,0");
  }

  // Navigate to verify page on startup (unless --skip-verify)
  if (!args.skip_verify()) {
    command_line->AppendArg("clawbrowser://verify/");
  }

  return base::ok(std::move(result));
}

}  // namespace clawbrowser
