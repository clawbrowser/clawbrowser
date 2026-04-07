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
  runtime.user_agent = fingerprint.user_agent;
  runtime.platform = fingerprint.platform;
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

  return runtime;
}

std::optional<RuntimeProxyConfig> ToRuntimeProxyConfig(
    const std::optional<ProxyConfig>& proxy) {
  if (!proxy.has_value()) {
    return std::nullopt;
  }

  RuntimeProxyConfig runtime_proxy;
  runtime_proxy.scheme = proxy->scheme;
  runtime_proxy.country = proxy->country;
  runtime_proxy.city = proxy->city;
  runtime_proxy.connection_type = proxy->connection_type;
  runtime_proxy.host = proxy->host;
  runtime_proxy.port = proxy->port;
  runtime_proxy.username = proxy->username;
  runtime_proxy.password = proxy->password;
  return runtime_proxy;
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

base::expected<std::string, std::string> BuildChildFingerprintPayload(
    const base::FilePath& path) {
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
  if (envelope->response.proxy.has_value()) {
    payload_dict.Set("proxy", envelope->response.proxy->ToDict());
  }

  std::string payload_json;
  if (!base::JSONWriter::Write(base::Value(std::move(payload_dict)),
                               &payload_json)) {
    return base::unexpected("failed to serialize child fingerprint payload");
  }

  std::string payload = base::Base64Encode(payload_json);
  return base::ok(std::move(payload));
}

base::expected<void, std::string> LoadFingerprintFromCommandLine(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(kFingerprintChildDataSwitch)) {
    auto child_result = LoadFingerprintFromChildPayload(
        command_line.GetSwitchValueASCII(kFingerprintChildDataSwitch));
    if (child_result.has_value()) {
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
  return LoadFingerprint(path);
}

}  // namespace clawbrowser
