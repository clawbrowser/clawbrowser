#include "clawbrowser/verify/verify_page.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "clawbrowser/cli/api_client.h"
#include "clawbrowser/cli/args.h"
#include "clawbrowser/cli/profile_manager.h"
#include "clawbrowser/fingerprint_accessor.h"
#include "clawbrowser/grit/clawbrowser_verify_resources.h"
#include "clawbrowser/paths.h"
#include "clawbrowser/verify/proxy_expectation.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_contents.h"

namespace clawbrowser {

ProxyExpectation::ProxyExpectation() = default;
ProxyExpectation::ProxyExpectation(const ProxyExpectation&) = default;
ProxyExpectation& ProxyExpectation::operator=(const ProxyExpectation&) =
    default;
ProxyExpectation::ProxyExpectation(ProxyExpectation&&) = default;
ProxyExpectation& ProxyExpectation::operator=(ProxyExpectation&&) = default;
ProxyExpectation::~ProxyExpectation() = default;

namespace {

struct VerifyApiConfig {
  std::optional<std::string> api_key;
  std::optional<std::string> base_url;
};

VerifyApiConfig LoadVerifyApiConfig(base::FilePath config_dir) {
  ProfileManager profile_manager(config_dir);
  VerifyApiConfig config;
  config.api_key = profile_manager.ResolveApiKey();
  config.base_url = profile_manager.ResolveBaseUrl();
  return config;
}

ProxyExpectation LoadVerifyExpectedProxyLocation(
    const base::FilePath& config_dir,
    const std::optional<std::string>& generated_country) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line) {
    return ResolveProxyExpectation(std::string(), std::nullopt,
                                   generated_country);
  }

  const std::string fingerprint_id =
      command_line->GetSwitchValueASCII("fingerprint");
  if (fingerprint_id.empty()) {
    return ResolveProxyExpectation(std::string(), std::nullopt,
                                   generated_country);
  }

  ProfileManager profile_manager(config_dir);
  auto profile = profile_manager.ReadProfile(fingerprint_id);
  if (!profile.has_value()) {
    return ResolveProxyExpectation(std::string(), std::nullopt,
                                   generated_country);
  }

  return ResolveProxyExpectation(profile->request.country,
                                 profile->request.city, generated_country);
}

std::string SerializeJson(base::ListValue list) {
  std::string json = "[]";
  base::JSONWriter::Write(base::Value(std::move(list)), &json);
  return json;
}

scoped_refptr<network::SharedURLLoaderFactory> GetVerifyApiUrlLoaderFactory(
    content::WebUI* web_ui) {
  if (g_browser_process &&
      g_browser_process->system_network_context_manager()) {
    auto factory =
        g_browser_process->system_network_context_manager()
            ->GetSharedURLLoaderFactory();
    if (factory) {
      return factory;
    }
  }

  if (!web_ui) {
    return nullptr;
  }

  content::WebContents* web_contents = web_ui->GetWebContents();
  if (!web_contents) {
    return nullptr;
  }

  auto* browser_context = web_contents->GetBrowserContext();
  if (!browser_context) {
    return nullptr;
  }

  auto* storage_partition = browser_context->GetDefaultStoragePartition();
  if (!storage_partition) {
    return nullptr;
  }

  return storage_partition->GetURLLoaderFactoryForBrowserProcess();
}

base::DictValue SerializeMediaDevice(const RuntimeMediaDevice& device) {
  base::DictValue item;
  item.Set("kind", device.kind.value_or(std::string()));
  item.Set("label", device.label.value_or(std::string()));
  item.Set("device_id", device.device_id.value_or(std::string()));
  return item;
}

base::DictValue SerializePlugin(const RuntimePlugin& plugin) {
  base::DictValue item;
  item.Set("name", plugin.name.value_or(std::string()));
  item.Set("description", plugin.description.value_or(std::string()));
  item.Set("filename", plugin.filename.value_or(std::string()));
  return item;
}

}  // namespace

bool VerifyFailureExitEnabledForCommandLine(
    const base::CommandLine& command_line) {
  return command_line.HasSwitch("verify-automation");
}

int ManagedProxyPrivacyCapabilityForCommandLine(
    const base::CommandLine& command_line,
    bool fingerprint_proxy_loaded) {
  constexpr char kRequiredWebRtcPolicy[] = "disable_non_proxied_udp";
  if (!fingerprint_proxy_loaded ||
      !command_line.HasSwitch(kRequireProxySwitch) ||
      command_line.GetSwitchValueASCII("proxy-server").empty() ||
      command_line.GetSwitchValueASCII("webrtc-ip-handling-policy") !=
          kRequiredWebRtcPolicy ||
      command_line.GetSwitchValueASCII("force-webrtc-ip-handling-policy") !=
          kRequiredWebRtcPolicy ||
      command_line.HasSwitch("no-proxy-server") ||
      command_line.HasSwitch("proxy-pac-url") ||
      command_line.HasSwitch("proxy-auto-detect") ||
      command_line.HasSwitch("proxy-bypass-list")) {
    return 0;
  }

  return 2;
}

VerifyPageUI::VerifyPageUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  automation_mode_ = VerifyFailureExitEnabledForCommandLine(
      *base::CommandLine::ForCurrentProcess());
  content::WebUIDataSource* source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(), kVerifyHost);
  source->SetSupportedScheme("clawbrowser");
  SetupDataSource(source);
  web_ui->AddRequestableScheme("clawbrowser");

  web_ui->RegisterMessageCallback(
      "verifyProxy",
      base::BindRepeating(&VerifyPageUI::HandleVerifyProxy,
                          base::Unretained(this)));
  web_ui->RegisterMessageCallback(
      "verifyComplete",
      base::BindRepeating(&VerifyPageUI::HandleVerifyComplete,
                          base::Unretained(this)));
}

VerifyPageUI::~VerifyPageUI() = default;

void VerifyPageUI::SetupDataSource(content::WebUIDataSource* source) {
  // Resources are embedded at compile time via grit/grd
  source->AddResourcePath("verify.html", IDR_CLAWBROWSER_VERIFY_HTML);
  source->AddResourcePath("verify_timezones.js",
                          IDR_CLAWBROWSER_VERIFY_TIMEZONES_JS);
  source->AddResourcePath("verify.js", IDR_CLAWBROWSER_VERIFY_JS);
  source->AddResourcePath("verify.css", IDR_CLAWBROWSER_VERIFY_CSS);
  source->AddResourcePath("side-bite.svg", IDR_CLAWBROWSER_SIDE_BITE_SVG);
  source->SetDefaultResource(IDR_CLAWBROWSER_VERIFY_HTML);

  source->AddString("has_expected_values", "false");
  source->AddString("has_proxy", "false");
  source->AddString("user_agent", "");
  source->AddString("platform", "");
  source->AddString("language_primary", "");
  source->AddString("languages_json", "[]");
  source->AddString("hardware_concurrency", "0");
  source->AddString("device_memory", "0");
  source->AddString("screen_width", "0");
  source->AddString("screen_height", "0");
  source->AddString("screen_avail_width", "0");
  source->AddString("screen_avail_height", "0");
  source->AddString("screen_color_depth", "0");
  source->AddString("pixel_ratio", "0");
  source->AddString("timezone", "");
  source->AddString("canvas_policy", "native");
  source->AddString("canvas_spoofing_enabled", "false");
  source->AddString("webgl_policy", "native");
  source->AddString("webgl_spoofing_enabled", "false");
  source->AddString("webgl_vendor", "");
  source->AddString("webgl_renderer", "");
  source->AddString("fonts", "[]");
  source->AddString("media_devices_json", "");
  source->AddString("media_devices_count", "0");
  source->AddString("plugins_json", "");
  source->AddString("plugins_count", "0");
  source->AddString("battery_charging", "");
  source->AddString("battery_level", "");
  source->AddString("speech_voices_json", "");
  source->AddString("speech_voices_count", "0");
  source->AddString("managed_proxy_privacy", "0");

  // Inject expected fingerprint values as replacements in the HTML.
  const RuntimeFingerprint* fp = FingerprintAccessor::Get();
  if (!fp)
    return;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const int managed_proxy_privacy =
      command_line
          ? ManagedProxyPrivacyCapabilityForCommandLine(
                *command_line, FingerprintAccessor::GetProxy() != nullptr)
          : 0;
  source->AddString("managed_proxy_privacy",
                    base::NumberToString(managed_proxy_privacy));

  source->AddString("has_expected_values", "true");
  source->AddString("has_proxy",
                    FingerprintAccessor::GetProxy() ? "true" : "false");
  source->AddString("user_agent", fp->user_agent);
  source->AddString("platform", fp->platform);
  source->AddString("language_primary",
                     fp->language.empty() ? "" : fp->language[0]);

  // Languages as JSON array string for comparison
  base::ListValue lang_list;
  for (const auto& lang : fp->language)
    lang_list.Append(lang);
  std::string languages_json;
  base::JSONWriter::Write(base::Value(std::move(lang_list)),
                          &languages_json);
  source->AddString("languages_json", languages_json);

  source->AddString("hardware_concurrency",
                     base::NumberToString(fp->hardware.concurrency));
  source->AddString("device_memory",
                     base::NumberToString(fp->hardware.memory));
  source->AddString("screen_width",
                     base::NumberToString(fp->screen.width));
  source->AddString("screen_height",
                     base::NumberToString(fp->screen.height));
  source->AddString("screen_avail_width",
                     base::NumberToString(fp->screen.avail_width));
  source->AddString("screen_avail_height",
                     base::NumberToString(fp->screen.avail_height));
  source->AddString("screen_color_depth",
                     base::NumberToString(fp->screen.color_depth));
  source->AddString("pixel_ratio",
                     base::NumberToString(fp->screen.pixel_ratio));
  source->AddString("timezone", fp->timezone);
  source->AddString("canvas_policy", fp->surface_policy.canvas);
  source->AddString("canvas_spoofing_enabled",
                    fp->canvas_spoofing_enabled ? "true" : "false");
  source->AddString("webgl_policy", fp->surface_policy.webgl);
  source->AddString("webgl_spoofing_enabled",
                    fp->webgl_spoofing_enabled ? "true" : "false");
  source->AddString("webgl_vendor", fp->webgl.vendor);
  source->AddString("webgl_renderer", fp->webgl.renderer);

  // Fonts as JSON array
  base::ListValue fonts_list;
  for (const auto& font : fp->fonts)
    fonts_list.Append(font);
  source->AddString("fonts", SerializeJson(std::move(fonts_list)));

  base::ListValue media_devices_list;
  for (const auto& device : fp->media_devices)
    media_devices_list.Append(SerializeMediaDevice(device));
  if (!fp->media_devices.empty()) {
    source->AddString("media_devices_json",
                       SerializeJson(std::move(media_devices_list)));
  }

  source->AddString("media_devices_count",
                     base::NumberToString(fp->media_devices.size()));

  base::ListValue plugins_list;
  for (const auto& plugin : fp->plugins)
    plugins_list.Append(SerializePlugin(plugin));
  if (!fp->plugins.empty())
    source->AddString("plugins_json", SerializeJson(std::move(plugins_list)));

  source->AddString("plugins_count",
                     base::NumberToString(fp->plugins.size()));

  if (fp->battery) {
    if (fp->battery->charging.has_value()) {
      source->AddString("battery_charging",
                        *fp->battery->charging ? "true" : "false");
    }
    if (fp->battery->level.has_value()) {
      source->AddString("battery_level",
                        base::NumberToString(*fp->battery->level));
    }
  }

  base::ListValue speech_voices_list;
  for (const auto& voice : fp->speech_voices)
    speech_voices_list.Append(voice);
  if (!fp->speech_voices.empty()) {
    source->AddString("speech_voices_json",
                       SerializeJson(std::move(speech_voices_list)));
  }

  source->AddString("speech_voices_count",
                     base::NumberToString(fp->speech_voices.size()));
}

void VerifyPageUI::HandleVerifyProxy(const base::ListValue& args) {
  auto send_result = [this](base::DictValue result) {
    web_ui()->CallJavascriptFunctionUnsafe("onProxyVerifyResult",
                                           base::Value(std::move(result)));
  };

  const RuntimeProxyConfig* proxy = FingerprintAccessor::GetProxy();
  if (!proxy) {
    // No proxy configured — skip proxy verification
    base::DictValue result;
    result.Set("match", true);
    result.Set("actual_country", "N/A");
    result.Set("detail", "no proxy configured");
    send_result(std::move(result));
    return;
  }

  const std::string proxy_scheme = proxy->scheme.value_or("http");
  const ProxyExpectation expected = LoadVerifyExpectedProxyLocation(
      GetClawbrowserConfigDir(), proxy->country);

  if (!proxy->host.has_value() || !proxy->port.has_value() ||
      !proxy->country.has_value() || !proxy->username.has_value() ||
      !proxy->password.has_value()) {
    base::DictValue result;
    result.Set("match", true);
    result.Set("actual_country", proxy->country.value_or("N/A"));
    result.Set("scheme", proxy_scheme);
    result.Set("error_code", "incomplete_proxy_config");
    result.Set("detail", "incomplete proxy config");
    if (expected.country.has_value())
      result.Set("expected_country", *expected.country);
    if (expected.city.has_value())
      result.Set("expected_city", *expected.city);
    send_result(std::move(result));
    return;
  }

  VerifyProxyRequest request;
  request.proxy.scheme = proxy_scheme;
  request.proxy.host = *proxy->host;
  request.proxy.port = *proxy->port;
  request.proxy.username = *proxy->username;
  request.proxy.password = *proxy->password;
  request.expected_country = expected.country.value_or(*proxy->country);
  if (expected.city.has_value())
    request.expected_city = *expected.city;

  std::string expected_country = request.expected_country;
  std::optional<std::string> expected_city = request.expected_city;
  std::string request_scheme = request.proxy.scheme.value_or("http");

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&LoadVerifyApiConfig, GetClawbrowserConfigDir()),
      base::BindOnce(
          [](base::WeakPtr<VerifyPageUI> self,
             VerifyProxyRequest request,
             std::string expected_country,
             std::optional<std::string> expected_city,
             std::string request_scheme,
             VerifyApiConfig config) {
            if (!self)
              return;

            auto send_async_result =
                [self, &expected_country, &expected_city,
                 &request_scheme](base::DictValue result) {
                  result.Set("expected_country", expected_country);
                  if (expected_city.has_value())
                    result.Set("expected_city", *expected_city);
                  result.Set("scheme", request_scheme);
                  self->web_ui()->CallJavascriptFunctionUnsafe(
                      "onProxyVerifyResult", base::Value(std::move(result)));
                };

            if (!config.api_key.has_value()) {
              base::DictValue result;
              result.Set("match", false);
              result.Set("actual_country", "N/A");
              result.Set("error_code", "missing_api_key");
              result.Set("detail", "API key not found");
              send_async_result(std::move(result));
              return;
            }

            if (!config.base_url.has_value()) {
              base::DictValue result;
              result.Set("match", false);
              result.Set("actual_country", "N/A");
              result.Set("error_code", "missing_api_base_url");
              result.Set("detail", "API base URL not found");
              send_async_result(std::move(result));
              return;
            }

            auto url_loader_factory =
                GetVerifyApiUrlLoaderFactory(self->web_ui());
            if (!url_loader_factory) {
              base::DictValue result;
              result.Set("match", false);
              result.Set("actual_country", "N/A");
              result.Set("error_code", "url_loader_unavailable");
              result.Set("detail", "URL loader factory unavailable");
              send_async_result(std::move(result));
              return;
            }

            self->api_client_ = std::make_unique<ApiClient>(
                *config.base_url, *config.api_key,
                std::move(url_loader_factory));
            self->api_client_->VerifyProxy(
                request,
                base::BindOnce(
                    [](base::WeakPtr<VerifyPageUI> self,
                       std::string expected_country,
                       std::optional<std::string> expected_city,
                       std::string request_scheme,
                       base::expected<VerifyProxyResponse, ApiError> response) {
                      if (!self)
                        return;

                      base::DictValue result;
                      result.Set("expected_country", expected_country);
                      if (expected_city.has_value())
                        result.Set("expected_city", *expected_city);
                      result.Set("scheme", request_scheme);

                      if (response.has_value()) {
                        result.Set("match", response->match);
                        result.Set("actual_country", response->actual_country);
                        if (response->actual_city.has_value())
                          result.Set("actual_city", *response->actual_city);
                        if (response->ipv4.has_value())
                          result.Set("ipv4", *response->ipv4);
                        if (response->ipv6.has_value())
                          result.Set("ipv6", *response->ipv6);
                      } else {
                        result.Set("match", false);
                        result.Set("actual_country", "N/A");
                        result.Set("error_code", response.error().code);
                        result.Set("detail", response.error().message);
                      }

                      self->api_client_.reset();
                      self->web_ui()->CallJavascriptFunctionUnsafe(
                          "onProxyVerifyResult", base::Value(std::move(result)));
                    },
                    self, std::move(expected_country), std::move(expected_city),
                    std::move(request_scheme)));
          },
          weak_ptr_factory_.GetWeakPtr(), request, std::move(expected_country),
          std::move(expected_city), std::move(request_scheme)));
}

void VerifyPageUI::HandleVerifyComplete(const base::ListValue& args) {
  if (args.empty() || !args[0].is_string())
    return;

  verification_complete_ = true;
  verify_passed_ = (args[0].GetString() == "pass");
  failure_exit_timer_.Stop();
  if (!verify_passed_)
    StartFailureExitTimer();
}

void VerifyPageUI::StartFailureExitTimer() {
  if (!automation_mode_ || !verification_complete_ || verify_passed_)
    return;

  // Spec: if verify fails and no CDP client connects within 30s, exit(1).
  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(
      web_ui()->GetWebContents());
  if (agent_host && agent_host->IsAttached()) {
    // CDP client is connected — don't force exit, let client handle it
    return;
  }

  failure_exit_timer_.Start(
      FROM_HERE, base::Seconds(30),
      base::BindOnce(&VerifyPageUI::OnFailureExitTimeout,
                     base::Unretained(this)));
}

void VerifyPageUI::OnFailureExitTimeout() {
  if (!automation_mode_) {
    return;
  }

  // 30s elapsed, verify failed, no CDP client connected
  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(
      web_ui()->GetWebContents());
  if (agent_host && agent_host->IsAttached()) {
    return;  // Client connected in the meantime
  }

  LOG(ERROR) << "[clawbrowser] verify failed, no CDP client connected "
             << "within 30s — exiting";
  base::Process::TerminateCurrentProcessImmediately(1);
}

}  // namespace clawbrowser
