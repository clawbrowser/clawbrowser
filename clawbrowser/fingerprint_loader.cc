#include "clawbrowser/fingerprint_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <memory>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "build/build_config.h"
#include "clawbrowser/cli/args.h"
#include "clawbrowser/fingerprint_accessor.h"
#include "clawbrowser/profile_envelope.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace clawbrowser {

namespace {

void ApplyTimezoneOverride(const Fingerprint& fingerprint) {
  if (fingerprint.timezone.empty()) {
    return;
  }

#if BUILDFLAG(IS_WIN)
  _putenv_s("TZ", fingerprint.timezone.c_str());
  _tzset();
#else
  setenv("TZ", fingerprint.timezone.c_str(), 1);
  tzset();
#endif

  icu::UnicodeString icu_timezone =
      icu::UnicodeString::fromUTF8(fingerprint.timezone);
  std::unique_ptr<icu::TimeZone> timezone(
      icu::TimeZone::createTimeZone(icu_timezone));
  icu::TimeZone::adoptDefault(timezone.release());
}

RuntimeFingerprint ToRuntimeFingerprint(const Fingerprint& fingerprint) {
  RuntimeFingerprint runtime;
  runtime.browser_family = fingerprint.browser_family;
  runtime.browser_version = fingerprint.browser_version;
  runtime.engine = fingerprint.engine;
  runtime.os = fingerprint.os;
  runtime.os_version = fingerprint.os_version;
  runtime.architecture = fingerprint.architecture;
  runtime.device_class = fingerprint.device_class;
  runtime.user_agent = fingerprint.user_agent;
  runtime.platform = fingerprint.platform;
  runtime.user_agent_data.platform = fingerprint.user_agent_data.platform;
  runtime.user_agent_data.platform_version =
      fingerprint.user_agent_data.platformVersion;
  runtime.user_agent_data.architecture =
      fingerprint.user_agent_data.architecture;
  runtime.user_agent_data.bitness = fingerprint.user_agent_data.bitness;
  runtime.user_agent_data.mobile = fingerprint.user_agent_data.mobile;
  runtime.user_agent_data.model = fingerprint.user_agent_data.model;
  runtime.user_agent_data.ua_full_version =
      fingerprint.user_agent_data.uaFullVersion;
  runtime.user_agent_data.brands.reserve(
      fingerprint.user_agent_data.brands.size());
  for (const auto& brand : fingerprint.user_agent_data.brands) {
    RuntimeClientHintBrand runtime_brand;
    runtime_brand.brand = brand.brand;
    runtime_brand.version = brand.version;
    runtime.user_agent_data.brands.push_back(std::move(runtime_brand));
  }
  runtime.user_agent_data.full_version_list.reserve(
      fingerprint.user_agent_data.fullVersionList.size());
  for (const auto& brand : fingerprint.user_agent_data.fullVersionList) {
    RuntimeClientHintBrand runtime_brand;
    runtime_brand.brand = brand.brand;
    runtime_brand.version = brand.version;
    runtime.user_agent_data.full_version_list.push_back(
        std::move(runtime_brand));
  }
  runtime.screen = {
      .width = fingerprint.screen.width,
      .height = fingerprint.screen.height,
      .avail_width = fingerprint.screen.avail_width,
      .avail_height = fingerprint.screen.avail_height,
      .color_depth = fingerprint.screen.color_depth,
      .pixel_ratio = fingerprint.screen.pixel_ratio,
  };
  runtime.hardware = {
      .concurrency = fingerprint.hardware.concurrency,
      .memory = fingerprint.hardware.memory,
  };
  runtime.webgl.vendor = fingerprint.webgl.vendor;
  runtime.webgl.renderer = fingerprint.webgl.renderer;
  runtime.canvas_seed = fingerprint.canvas_seed;
  runtime.audio_seed = fingerprint.audio_seed;
  runtime.client_rects_seed = fingerprint.client_rects_seed;
  runtime.timezone = fingerprint.timezone;
  runtime.language = fingerprint.language;
  runtime.fonts = fingerprint.fonts;
  runtime.speech_voices = fingerprint.speech_voices;

  runtime.media_devices.reserve(fingerprint.media_devices.size());
  for (const auto& device : fingerprint.media_devices) {
    RuntimeMediaDevice runtime_device;
    runtime_device.kind = device.kind;
    runtime_device.label = device.label;
    runtime_device.device_id = device.device_id;
    runtime.media_devices.push_back(std::move(runtime_device));
  }

  runtime.plugins.reserve(fingerprint.plugins.size());
  for (const auto& plugin : fingerprint.plugins) {
    RuntimePlugin runtime_plugin;
    runtime_plugin.name = plugin.name;
    runtime_plugin.description = plugin.description;
    runtime_plugin.filename = plugin.filename;
    runtime.plugins.push_back(std::move(runtime_plugin));
  }

  if (fingerprint.battery.has_value()) {
    RuntimeBattery runtime_battery;
    runtime_battery.charging = fingerprint.battery->charging;
    runtime_battery.level = fingerprint.battery->level;
    runtime.battery = std::move(runtime_battery);
  }

  runtime.audio_codecs = fingerprint.audio_codecs;
  runtime.video_codecs = fingerprint.video_codecs;
  runtime.headers = fingerprint.headers;
  runtime.surface_policy.canvas = fingerprint.surface_policy.canvas.mode;
  runtime.surface_policy.audio = fingerprint.surface_policy.audio.mode;
  runtime.surface_policy.client_rects =
      fingerprint.surface_policy.client_rects.mode;
  runtime.surface_policy.webgl = fingerprint.surface_policy.webgl.mode;
  runtime.surface_policy.fonts = fingerprint.surface_policy.fonts.mode;
  runtime.surface_policy.plugins = fingerprint.surface_policy.plugins.mode;
  runtime.surface_policy.media_devices =
      fingerprint.surface_policy.media_devices.mode;
  runtime.surface_policy.speech_voices =
      fingerprint.surface_policy.speech_voices.mode;

  return runtime;
}

std::optional<RuntimeProxyConfig> ToRuntimeProxyConfig(
    const std::optional<ProxyConfig>& proxy) {
  if (!proxy.has_value()) {
    return std::nullopt;
  }

  RuntimeProxyConfig runtime_proxy;
  runtime_proxy.scheme = proxy->scheme;
  if (!runtime_proxy.scheme.has_value() || runtime_proxy.scheme->empty()) {
    runtime_proxy.scheme = "http";
  }
  runtime_proxy.country = proxy->country;
  runtime_proxy.city = proxy->city;
  runtime_proxy.connection_type = proxy->connection_type;
  runtime_proxy.host = proxy->host;
  runtime_proxy.port = proxy->port;
  runtime_proxy.username = proxy->username;
  runtime_proxy.password = proxy->password;
  return runtime_proxy;
}

ProxyConfig ToChildProxyConfig(const RuntimeProxyConfig& runtime_proxy) {
  ProxyConfig child_proxy;
  child_proxy.scheme = runtime_proxy.scheme;
  child_proxy.country = runtime_proxy.country;
  child_proxy.city = runtime_proxy.city;
  child_proxy.connection_type = runtime_proxy.connection_type;
  child_proxy.host = runtime_proxy.host;
  child_proxy.port = runtime_proxy.port;
  return child_proxy;
}

ProxyConfig ToChildProxyConfig(const ProxyConfig& proxy) {
  ProxyConfig child_proxy = proxy;
  child_proxy.username.reset();
  child_proxy.password.reset();
  return child_proxy;
}

void ApplySpoofingPolicyFromCommandLine(
    const base::CommandLine& command_line) {
  FingerprintAccessor::SetSpoofingPolicy(
      command_line.HasSwitch(kEnableCanvasSpoofingSwitch) &&
          !command_line.HasSwitch(kDisableCanvasSpoofingSwitch),
      command_line.HasSwitch(kEnableWebGLSpoofingSwitch) &&
          !command_line.HasSwitch(kDisableWebGLSpoofingSwitch));
}

base::expected<void, std::string> LoadFingerprintContents(
    const std::string& contents) {
  auto envelope = ProfileEnvelope::Parse(contents);
  if (!envelope.has_value()) {
    return base::unexpected(envelope.error());
  }

  if (envelope->schema_outdated) {
    fprintf(stderr,
            "[clawbrowser] warn: fingerprint schema version %d is outdated, "
            "consider --regenerate\n",
            envelope->schema_version);
  }

  ApplyTimezoneOverride(envelope->response.fingerprint);
  FingerprintAccessor::Set(
      ToRuntimeFingerprint(envelope->response.fingerprint),
      ToRuntimeProxyConfig(envelope->response.proxy));
  return base::ok();
}

base::expected<void, std::string> LoadFingerprintFromChildPayload(
    const std::string& payload) {
  std::string json;
  if (!base::Base64Decode(payload, &json)) {
    return base::unexpected("failed to decode child fingerprint payload");
  }

  std::optional<base::Value> parsed =
      base::JSONReader::Read(json, base::JSON_PARSE_RFC);
  if (!parsed || !parsed->is_dict()) {
    return base::unexpected("failed to parse child fingerprint payload");
  }

  const base::DictValue& dict = parsed->GetDict();
  const base::DictValue* fingerprint_dict = dict.FindDict("fingerprint");
  if (!fingerprint_dict) {
    return base::unexpected("child fingerprint payload missing fingerprint");
  }

  auto fingerprint = Fingerprint::FromDict(*fingerprint_dict);
  if (!fingerprint.has_value()) {
    return base::unexpected("failed to parse child fingerprint payload: " +
                            fingerprint.error());
  }

  std::optional<ProxyConfig> proxy;
  if (const base::DictValue* proxy_dict = dict.FindDict("proxy")) {
    auto parsed_proxy = ProxyConfig::FromDict(*proxy_dict);
    if (!parsed_proxy.has_value()) {
      return base::unexpected("failed to parse child proxy payload: " +
                              parsed_proxy.error());
    }
    proxy = std::move(*parsed_proxy);
  }

  ApplyTimezoneOverride(*fingerprint);
  FingerprintAccessor::Set(ToRuntimeFingerprint(*fingerprint),
                          ToRuntimeProxyConfig(proxy));
  return base::ok();
}

}  // namespace

base::expected<void, std::string> LoadFingerprint(
    const base::FilePath& path) {
  std::string contents;
  if (!base::ReadFileToString(path, &contents)) {
    return base::unexpected(
        "failed to read fingerprint file: " + path.AsUTF8Unsafe());
  }

  return LoadFingerprintContents(contents);
}

base::expected<std::string, std::string> BuildChildFingerprintPayloadInternal(
    const base::FilePath& path,
    const RuntimeProxyConfig* proxy_override) {
  std::string contents;
  if (!base::ReadFileToString(path, &contents)) {
    return base::unexpected(
        "failed to read fingerprint file: " + path.AsUTF8Unsafe());
  }

  auto envelope = ProfileEnvelope::Parse(contents);
  if (!envelope.has_value()) {
    return base::unexpected(envelope.error());
  }

  base::DictValue payload_dict;
  payload_dict.Set("fingerprint", envelope->response.fingerprint.ToDict());
  if (proxy_override) {
    payload_dict.Set("proxy", ToChildProxyConfig(*proxy_override).ToDict());
  } else if (envelope->response.proxy.has_value()) {
    ProxyConfig child_proxy = ToChildProxyConfig(*envelope->response.proxy);
    payload_dict.Set("proxy", child_proxy.ToDict());
  }

  std::string payload_json;
  if (!base::JSONWriter::Write(base::Value(std::move(payload_dict)),
                               &payload_json)) {
    return base::unexpected("failed to serialize child fingerprint payload");
  }

  std::string payload = base::Base64Encode(payload_json);
  return base::ok(std::move(payload));
}

base::expected<std::string, std::string> BuildChildFingerprintPayload(
    const base::FilePath& path) {
  return BuildChildFingerprintPayloadInternal(path, nullptr);
}

base::expected<std::string, std::string> BuildChildFingerprintPayload(
    const base::FilePath& path,
    const RuntimeProxyConfig& proxy_override) {
  return BuildChildFingerprintPayloadInternal(path, &proxy_override);
}

base::expected<void, std::string> LoadFingerprintFromCommandLine(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(kFingerprintChildDataSwitch)) {
    auto child_result = LoadFingerprintFromChildPayload(
        command_line.GetSwitchValueASCII(kFingerprintChildDataSwitch));
    if (child_result.has_value()) {
      ApplySpoofingPolicyFromCommandLine(command_line);
      return base::ok();
    }
    if (!command_line.HasSwitch(kFingerprintPathSwitch)) {
      return child_result;
    }
  }

  if (!command_line.HasSwitch(kFingerprintPathSwitch)) {
    return base::ok();  // Vanilla mode — no fingerprint
  }

  base::FilePath path =
      command_line.GetSwitchValuePath(kFingerprintPathSwitch);
  auto result = LoadFingerprint(path);
  if (result.has_value()) {
    ApplySpoofingPolicyFromCommandLine(command_line);
  }
  return result;
}

}  // namespace clawbrowser
