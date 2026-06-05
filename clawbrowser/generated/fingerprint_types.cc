// Copyright 2026 The Clawbrowser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AUTO-GENERATED FILE — DO NOT EDIT MANUALLY.
// Source: clawbrowser/schemas/browser_schema.json
// See clawbrowser/generated/README.md for re-generation instructions.

#include "clawbrowser/generated/fingerprint_types.h"

#include <string_view>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"

namespace clawbrowser {

namespace {

base::expected<std::string, std::string> RequireString(
    const base::DictValue& dict,
    std::string_view key) {
  const std::string* value = dict.FindString(key);
  if (!value) {
    return base::unexpected(std::string("missing required string field: ") + std::string(key));
  }
  return base::ok(*value);
}

base::expected<int, std::string> RequireInt(
    const base::DictValue& dict,
    std::string_view key) {
  std::optional<int> value = dict.FindInt(key);
  if (!value.has_value()) {
    return base::unexpected(std::string("missing required int field: ") + std::string(key));
  }
  return base::ok(*value);
}

base::expected<bool, std::string> RequireBool(
    const base::DictValue& dict,
    std::string_view key) {
  std::optional<bool> value = dict.FindBool(key);
  if (!value.has_value()) {
    return base::unexpected(std::string("missing required bool field: ") + std::string(key));
  }
  return base::ok(*value);
}

base::expected<double, std::string> RequireDouble(
    const base::DictValue& dict,
    std::string_view key) {
  const base::Value* value = dict.Find(key);
  if (!value) {
    return base::unexpected(std::string("missing required number field: ") + std::string(key));
  }
  if (value->is_double()) return base::ok(value->GetDouble());
  if (value->is_int()) return base::ok(static_cast<double>(value->GetInt()));
  return base::unexpected(std::string("field is not a number: ") + std::string(key));
}

std::optional<double> FindOptionalDouble(
    const base::DictValue& dict,
    std::string_view key) {
  const base::Value* value = dict.Find(key);
  if (!value) return std::nullopt;
  if (value->is_double()) return value->GetDouble();
  if (value->is_int()) return static_cast<double>(value->GetInt());
  return std::nullopt;
}

base::expected<const base::DictValue*, std::string> RequireDict(
    const base::DictValue& dict,
    std::string_view key) {
  const base::DictValue* value = dict.FindDict(key);
  if (!value) {
    return base::unexpected(std::string("missing required object field: ") + std::string(key));
  }
  return base::ok(value);
}

base::expected<base::DictValue, std::string> ParseJsonToDict(
    const std::string& json) {
  auto parsed = base::JSONReader::ReadAndReturnValueWithError(
      json, base::JSON_PARSE_RFC);
  if (!parsed.has_value()) {
    return base::unexpected("JSON parse error: " + parsed.error().message);
  }
  if (!parsed->is_dict()) {
    return base::unexpected("expected a JSON object at root");
  }
  return base::ok(std::move(parsed->GetDict()));
}

std::string DictToJson(const base::DictValue& dict) {
  std::string output;
  base::JSONWriter::Write(base::Value(dict.Clone()), &output);
  return output;
}

}  // namespace

GenerateRequest::GenerateRequest() = default;
GenerateRequest::GenerateRequest(const GenerateRequest&) = default;
GenerateRequest& GenerateRequest::operator=(const GenerateRequest&) = default;
GenerateRequest::GenerateRequest(GenerateRequest&&) = default;
GenerateRequest& GenerateRequest::operator=(GenerateRequest&&) = default;
GenerateRequest::~GenerateRequest() = default;

base::expected<GenerateRequest, std::string> GenerateRequest::FromDict(
    const base::DictValue& dict) {
  GenerateRequest value;

  auto platform = RequireString(dict, "platform");
  if (!platform.has_value()) return base::unexpected(platform.error());
  value.platform = *platform;

  auto browser = RequireString(dict, "browser");
  if (!browser.has_value()) return base::unexpected(browser.error());
  value.browser = *browser;

  auto country = RequireString(dict, "country");
  if (!country.has_value()) return base::unexpected(country.error());
  value.country = *country;

  if (const std::string* parsed = dict.FindString("city")) value.city = *parsed;

  if (const std::string* parsed = dict.FindString("connection_type")) value.connection_type = *parsed;

  if (const std::string* parsed = dict.FindString("runtime_browser_version")) value.runtime_browser_version = *parsed;

  if (const std::string* parsed = dict.FindString("runtime_os")) value.runtime_os = *parsed;

  if (const std::string* parsed = dict.FindString("runtime_arch")) value.runtime_arch = *parsed;

  if (const std::string* parsed = dict.FindString("runtime_gpu")) value.runtime_gpu = *parsed;

  if (std::optional<bool> parsed = dict.FindBool("runtime_headless")) value.runtime_headless = *parsed;

  return base::ok(std::move(value));
}

base::expected<GenerateRequest, std::string> GenerateRequest::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue GenerateRequest::ToDict() const {
  base::DictValue dict;
  dict.Set("platform", platform);
  dict.Set("browser", browser);
  dict.Set("country", country);
  if (city.has_value()) dict.Set("city", *city);
  if (connection_type.has_value()) dict.Set("connection_type", *connection_type);
  if (runtime_browser_version.has_value()) dict.Set("runtime_browser_version", *runtime_browser_version);
  if (runtime_os.has_value()) dict.Set("runtime_os", *runtime_os);
  if (runtime_arch.has_value()) dict.Set("runtime_arch", *runtime_arch);
  if (runtime_gpu.has_value()) dict.Set("runtime_gpu", *runtime_gpu);
  if (runtime_headless.has_value()) dict.Set("runtime_headless", *runtime_headless);
  return dict;
}

std::string GenerateRequest::ToJson() const {
  return DictToJson(ToDict());
}

base::expected<Screen, std::string> Screen::FromDict(
    const base::DictValue& dict) {
  Screen value;

  auto width = RequireInt(dict, "width");
  if (!width.has_value()) return base::unexpected(width.error());
  value.width = *width;

  auto height = RequireInt(dict, "height");
  if (!height.has_value()) return base::unexpected(height.error());
  value.height = *height;

  auto avail_width = RequireInt(dict, "avail_width");
  if (!avail_width.has_value()) return base::unexpected(avail_width.error());
  value.avail_width = *avail_width;

  auto avail_height = RequireInt(dict, "avail_height");
  if (!avail_height.has_value()) return base::unexpected(avail_height.error());
  value.avail_height = *avail_height;

  auto color_depth = RequireInt(dict, "color_depth");
  if (!color_depth.has_value()) return base::unexpected(color_depth.error());
  value.color_depth = *color_depth;

  auto pixel_ratio = RequireDouble(dict, "pixel_ratio");
  if (!pixel_ratio.has_value()) return base::unexpected(pixel_ratio.error());
  value.pixel_ratio = *pixel_ratio;

  return base::ok(std::move(value));
}

base::expected<Screen, std::string> Screen::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue Screen::ToDict() const {
  base::DictValue dict;
  dict.Set("width", width);
  dict.Set("height", height);
  dict.Set("avail_width", avail_width);
  dict.Set("avail_height", avail_height);
  dict.Set("color_depth", color_depth);
  dict.Set("pixel_ratio", pixel_ratio);
  return dict;
}

std::string Screen::ToJson() const {
  return DictToJson(ToDict());
}

base::expected<Hardware, std::string> Hardware::FromDict(
    const base::DictValue& dict) {
  Hardware value;

  auto concurrency = RequireInt(dict, "concurrency");
  if (!concurrency.has_value()) return base::unexpected(concurrency.error());
  value.concurrency = *concurrency;

  auto memory = RequireInt(dict, "memory");
  if (!memory.has_value()) return base::unexpected(memory.error());
  value.memory = *memory;

  return base::ok(std::move(value));
}

base::expected<Hardware, std::string> Hardware::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue Hardware::ToDict() const {
  base::DictValue dict;
  dict.Set("concurrency", concurrency);
  dict.Set("memory", memory);
  return dict;
}

std::string Hardware::ToJson() const {
  return DictToJson(ToDict());
}

WebGL::WebGL() = default;
WebGL::WebGL(const WebGL&) = default;
WebGL& WebGL::operator=(const WebGL&) = default;
WebGL::WebGL(WebGL&&) = default;
WebGL& WebGL::operator=(WebGL&&) = default;
WebGL::~WebGL() = default;

base::expected<WebGL, std::string> WebGL::FromDict(
    const base::DictValue& dict) {
  WebGL value;

  auto vendor = RequireString(dict, "vendor");
  if (!vendor.has_value()) return base::unexpected(vendor.error());
  value.vendor = *vendor;

  auto renderer = RequireString(dict, "renderer");
  if (!renderer.has_value()) return base::unexpected(renderer.error());
  value.renderer = *renderer;

  return base::ok(std::move(value));
}

base::expected<WebGL, std::string> WebGL::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue WebGL::ToDict() const {
  base::DictValue dict;
  dict.Set("vendor", vendor);
  dict.Set("renderer", renderer);
  return dict;
}

std::string WebGL::ToJson() const {
  return DictToJson(ToDict());
}

MediaDevice::MediaDevice() = default;
MediaDevice::MediaDevice(const MediaDevice&) = default;
MediaDevice& MediaDevice::operator=(const MediaDevice&) = default;
MediaDevice::MediaDevice(MediaDevice&&) = default;
MediaDevice& MediaDevice::operator=(MediaDevice&&) = default;
MediaDevice::~MediaDevice() = default;

base::expected<MediaDevice, std::string> MediaDevice::FromDict(
    const base::DictValue& dict) {
  MediaDevice value;

  if (const std::string* parsed = dict.FindString("kind")) value.kind = *parsed;

  if (const std::string* parsed = dict.FindString("label")) value.label = *parsed;

  if (const std::string* parsed = dict.FindString("device_id")) value.device_id = *parsed;

  return base::ok(std::move(value));
}

base::expected<MediaDevice, std::string> MediaDevice::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue MediaDevice::ToDict() const {
  base::DictValue dict;
  if (kind.has_value()) dict.Set("kind", *kind);
  if (label.has_value()) dict.Set("label", *label);
  if (device_id.has_value()) dict.Set("device_id", *device_id);
  return dict;
}

std::string MediaDevice::ToJson() const {
  return DictToJson(ToDict());
}

Plugin::Plugin() = default;
Plugin::Plugin(const Plugin&) = default;
Plugin& Plugin::operator=(const Plugin&) = default;
Plugin::Plugin(Plugin&&) = default;
Plugin& Plugin::operator=(Plugin&&) = default;
Plugin::~Plugin() = default;

base::expected<Plugin, std::string> Plugin::FromDict(
    const base::DictValue& dict) {
  Plugin value;

  if (const std::string* parsed = dict.FindString("name")) value.name = *parsed;

  if (const std::string* parsed = dict.FindString("description")) value.description = *parsed;

  if (const std::string* parsed = dict.FindString("filename")) value.filename = *parsed;

  return base::ok(std::move(value));
}

base::expected<Plugin, std::string> Plugin::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue Plugin::ToDict() const {
  base::DictValue dict;
  if (name.has_value()) dict.Set("name", *name);
  if (description.has_value()) dict.Set("description", *description);
  if (filename.has_value()) dict.Set("filename", *filename);
  return dict;
}

std::string Plugin::ToJson() const {
  return DictToJson(ToDict());
}

Battery::Battery() = default;
Battery::Battery(const Battery&) = default;
Battery& Battery::operator=(const Battery&) = default;
Battery::Battery(Battery&&) = default;
Battery& Battery::operator=(Battery&&) = default;
Battery::~Battery() = default;

base::expected<Battery, std::string> Battery::FromDict(
    const base::DictValue& dict) {
  Battery value;

  if (std::optional<bool> parsed = dict.FindBool("charging")) value.charging = *parsed;

  if (std::optional<double> parsed = FindOptionalDouble(dict, "level")) value.level = *parsed;

  return base::ok(std::move(value));
}

base::expected<Battery, std::string> Battery::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue Battery::ToDict() const {
  base::DictValue dict;
  if (charging.has_value()) dict.Set("charging", *charging);
  if (level.has_value()) dict.Set("level", *level);
  return dict;
}

std::string Battery::ToJson() const {
  return DictToJson(ToDict());
}

ClientHintBrand::ClientHintBrand() = default;
ClientHintBrand::ClientHintBrand(const ClientHintBrand&) = default;
ClientHintBrand& ClientHintBrand::operator=(const ClientHintBrand&) = default;
ClientHintBrand::ClientHintBrand(ClientHintBrand&&) = default;
ClientHintBrand& ClientHintBrand::operator=(ClientHintBrand&&) = default;
ClientHintBrand::~ClientHintBrand() = default;

base::expected<ClientHintBrand, std::string> ClientHintBrand::FromDict(
    const base::DictValue& dict) {
  ClientHintBrand value;

  auto brand = RequireString(dict, "brand");
  if (!brand.has_value()) return base::unexpected(brand.error());
  value.brand = *brand;

  auto version = RequireString(dict, "version");
  if (!version.has_value()) return base::unexpected(version.error());
  value.version = *version;

  return base::ok(std::move(value));
}

base::expected<ClientHintBrand, std::string> ClientHintBrand::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue ClientHintBrand::ToDict() const {
  base::DictValue dict;
  dict.Set("brand", brand);
  dict.Set("version", version);
  return dict;
}

std::string ClientHintBrand::ToJson() const {
  return DictToJson(ToDict());
}

UserAgentData::UserAgentData() = default;
UserAgentData::UserAgentData(const UserAgentData&) = default;
UserAgentData& UserAgentData::operator=(const UserAgentData&) = default;
UserAgentData::UserAgentData(UserAgentData&&) = default;
UserAgentData& UserAgentData::operator=(UserAgentData&&) = default;
UserAgentData::~UserAgentData() = default;

base::expected<UserAgentData, std::string> UserAgentData::FromDict(
    const base::DictValue& dict) {
  UserAgentData value;

  const base::ListValue* brands_list = dict.FindList("brands");
  if (!brands_list) return base::unexpected("missing required array field: brands");
  for (const base::Value& item : *brands_list) {
    if (!item.is_dict()) {
      return base::unexpected("brands array contains non-object element");
    }
    auto parsed_item = ClientHintBrand::FromDict(item.GetDict());
    if (!parsed_item.has_value()) return base::unexpected(parsed_item.error());
    value.brands.push_back(std::move(*parsed_item));
  }

  const base::ListValue* fullVersionList_list = dict.FindList("fullVersionList");
  if (!fullVersionList_list) return base::unexpected("missing required array field: fullVersionList");
  for (const base::Value& item : *fullVersionList_list) {
    if (!item.is_dict()) {
      return base::unexpected("fullVersionList array contains non-object element");
    }
    auto parsed_item = ClientHintBrand::FromDict(item.GetDict());
    if (!parsed_item.has_value()) return base::unexpected(parsed_item.error());
    value.fullVersionList.push_back(std::move(*parsed_item));
  }

  auto platform = RequireString(dict, "platform");
  if (!platform.has_value()) return base::unexpected(platform.error());
  value.platform = *platform;

  auto platformVersion = RequireString(dict, "platformVersion");
  if (!platformVersion.has_value()) return base::unexpected(platformVersion.error());
  value.platformVersion = *platformVersion;

  auto architecture = RequireString(dict, "architecture");
  if (!architecture.has_value()) return base::unexpected(architecture.error());
  value.architecture = *architecture;

  auto bitness = RequireString(dict, "bitness");
  if (!bitness.has_value()) return base::unexpected(bitness.error());
  value.bitness = *bitness;

  auto mobile = RequireBool(dict, "mobile");
  if (!mobile.has_value()) return base::unexpected(mobile.error());
  value.mobile = *mobile;

  auto model = RequireString(dict, "model");
  if (!model.has_value()) return base::unexpected(model.error());
  value.model = *model;

  if (const std::string* parsed = dict.FindString("uaFullVersion")) value.uaFullVersion = *parsed;

  return base::ok(std::move(value));
}

base::expected<UserAgentData, std::string> UserAgentData::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue UserAgentData::ToDict() const {
  base::DictValue dict;
  {
    base::ListValue list;
    for (const auto& item : brands) list.Append(item.ToDict());
    dict.Set("brands", std::move(list));
  }
  {
    base::ListValue list;
    for (const auto& item : fullVersionList) list.Append(item.ToDict());
    dict.Set("fullVersionList", std::move(list));
  }
  dict.Set("platform", platform);
  dict.Set("platformVersion", platformVersion);
  dict.Set("architecture", architecture);
  dict.Set("bitness", bitness);
  dict.Set("mobile", mobile);
  dict.Set("model", model);
  if (uaFullVersion.has_value()) dict.Set("uaFullVersion", *uaFullVersion);
  return dict;
}

std::string UserAgentData::ToJson() const {
  return DictToJson(ToDict());
}

SurfacePolicyRule::SurfacePolicyRule() = default;
SurfacePolicyRule::SurfacePolicyRule(const SurfacePolicyRule&) = default;
SurfacePolicyRule& SurfacePolicyRule::operator=(const SurfacePolicyRule&) = default;
SurfacePolicyRule::SurfacePolicyRule(SurfacePolicyRule&&) = default;
SurfacePolicyRule& SurfacePolicyRule::operator=(SurfacePolicyRule&&) = default;
SurfacePolicyRule::~SurfacePolicyRule() = default;

base::expected<SurfacePolicyRule, std::string> SurfacePolicyRule::FromDict(
    const base::DictValue& dict) {
  SurfacePolicyRule value;

  auto mode = RequireString(dict, "mode");
  if (!mode.has_value()) return base::unexpected(mode.error());
  value.mode = *mode;

  return base::ok(std::move(value));
}

base::expected<SurfacePolicyRule, std::string> SurfacePolicyRule::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue SurfacePolicyRule::ToDict() const {
  base::DictValue dict;
  dict.Set("mode", mode);
  return dict;
}

std::string SurfacePolicyRule::ToJson() const {
  return DictToJson(ToDict());
}

SurfacePolicy::SurfacePolicy() = default;
SurfacePolicy::SurfacePolicy(const SurfacePolicy&) = default;
SurfacePolicy& SurfacePolicy::operator=(const SurfacePolicy&) = default;
SurfacePolicy::SurfacePolicy(SurfacePolicy&&) = default;
SurfacePolicy& SurfacePolicy::operator=(SurfacePolicy&&) = default;
SurfacePolicy::~SurfacePolicy() = default;

base::expected<SurfacePolicy, std::string> SurfacePolicy::FromDict(
    const base::DictValue& dict) {
  SurfacePolicy value;

  auto canvas_dict = RequireDict(dict, "canvas");
  if (!canvas_dict.has_value()) return base::unexpected(canvas_dict.error());
  auto canvas = SurfacePolicyRule::FromDict(**canvas_dict);
  if (!canvas.has_value()) return base::unexpected(canvas.error());
  value.canvas = std::move(*canvas);

  auto audio_dict = RequireDict(dict, "audio");
  if (!audio_dict.has_value()) return base::unexpected(audio_dict.error());
  auto audio = SurfacePolicyRule::FromDict(**audio_dict);
  if (!audio.has_value()) return base::unexpected(audio.error());
  value.audio = std::move(*audio);

  auto client_rects_dict = RequireDict(dict, "client_rects");
  if (!client_rects_dict.has_value()) return base::unexpected(client_rects_dict.error());
  auto client_rects = SurfacePolicyRule::FromDict(**client_rects_dict);
  if (!client_rects.has_value()) return base::unexpected(client_rects.error());
  value.client_rects = std::move(*client_rects);

  auto webgl_dict = RequireDict(dict, "webgl");
  if (!webgl_dict.has_value()) return base::unexpected(webgl_dict.error());
  auto webgl = SurfacePolicyRule::FromDict(**webgl_dict);
  if (!webgl.has_value()) return base::unexpected(webgl.error());
  value.webgl = std::move(*webgl);

  auto fonts_dict = RequireDict(dict, "fonts");
  if (!fonts_dict.has_value()) return base::unexpected(fonts_dict.error());
  auto fonts = SurfacePolicyRule::FromDict(**fonts_dict);
  if (!fonts.has_value()) return base::unexpected(fonts.error());
  value.fonts = std::move(*fonts);

  auto plugins_dict = RequireDict(dict, "plugins");
  if (!plugins_dict.has_value()) return base::unexpected(plugins_dict.error());
  auto plugins = SurfacePolicyRule::FromDict(**plugins_dict);
  if (!plugins.has_value()) return base::unexpected(plugins.error());
  value.plugins = std::move(*plugins);

  auto media_devices_dict = RequireDict(dict, "media_devices");
  if (!media_devices_dict.has_value()) return base::unexpected(media_devices_dict.error());
  auto media_devices = SurfacePolicyRule::FromDict(**media_devices_dict);
  if (!media_devices.has_value()) return base::unexpected(media_devices.error());
  value.media_devices = std::move(*media_devices);

  auto speech_voices_dict = RequireDict(dict, "speech_voices");
  if (!speech_voices_dict.has_value()) return base::unexpected(speech_voices_dict.error());
  auto speech_voices = SurfacePolicyRule::FromDict(**speech_voices_dict);
  if (!speech_voices.has_value()) return base::unexpected(speech_voices.error());
  value.speech_voices = std::move(*speech_voices);

  return base::ok(std::move(value));
}

base::expected<SurfacePolicy, std::string> SurfacePolicy::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue SurfacePolicy::ToDict() const {
  base::DictValue dict;
  dict.Set("canvas", canvas.ToDict());
  dict.Set("audio", audio.ToDict());
  dict.Set("client_rects", client_rects.ToDict());
  dict.Set("webgl", webgl.ToDict());
  dict.Set("fonts", fonts.ToDict());
  dict.Set("plugins", plugins.ToDict());
  dict.Set("media_devices", media_devices.ToDict());
  dict.Set("speech_voices", speech_voices.ToDict());
  return dict;
}

std::string SurfacePolicy::ToJson() const {
  return DictToJson(ToDict());
}

GeneratorProvenance::GeneratorProvenance() = default;
GeneratorProvenance::GeneratorProvenance(const GeneratorProvenance&) = default;
GeneratorProvenance& GeneratorProvenance::operator=(const GeneratorProvenance&) = default;
GeneratorProvenance::GeneratorProvenance(GeneratorProvenance&&) = default;
GeneratorProvenance& GeneratorProvenance::operator=(GeneratorProvenance&&) = default;
GeneratorProvenance::~GeneratorProvenance() = default;

base::expected<GeneratorProvenance, std::string> GeneratorProvenance::FromDict(
    const base::DictValue& dict) {
  GeneratorProvenance value;

  auto provider = RequireString(dict, "provider");
  if (!provider.has_value()) return base::unexpected(provider.error());
  value.provider = *provider;

  auto version = RequireString(dict, "version");
  if (!version.has_value()) return base::unexpected(version.error());
  value.version = *version;

  auto schema_version = RequireInt(dict, "schema_version");
  if (!schema_version.has_value()) return base::unexpected(schema_version.error());
  value.schema_version = *schema_version;

  return base::ok(std::move(value));
}

base::expected<GeneratorProvenance, std::string> GeneratorProvenance::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue GeneratorProvenance::ToDict() const {
  base::DictValue dict;
  dict.Set("provider", provider);
  dict.Set("version", version);
  dict.Set("schema_version", schema_version);
  return dict;
}

std::string GeneratorProvenance::ToJson() const {
  return DictToJson(ToDict());
}

ProxyConfig::ProxyConfig() = default;
ProxyConfig::ProxyConfig(const ProxyConfig&) = default;
ProxyConfig& ProxyConfig::operator=(const ProxyConfig&) = default;
ProxyConfig::ProxyConfig(ProxyConfig&&) = default;
ProxyConfig& ProxyConfig::operator=(ProxyConfig&&) = default;
ProxyConfig::~ProxyConfig() = default;

base::expected<ProxyConfig, std::string> ProxyConfig::FromDict(
    const base::DictValue& dict) {
  ProxyConfig value;

  if (const std::string* parsed = dict.FindString("country")) value.country = *parsed;

  if (const std::string* parsed = dict.FindString("city")) value.city = *parsed;

  if (const std::string* parsed = dict.FindString("connection_type")) value.connection_type = *parsed;

  if (const std::string* parsed = dict.FindString("host")) value.host = *parsed;

  if (std::optional<int> parsed = dict.FindInt("port")) value.port = *parsed;

  if (const std::string* parsed = dict.FindString("username")) value.username = *parsed;

  if (const std::string* parsed = dict.FindString("password")) value.password = *parsed;

  return base::ok(std::move(value));
}

base::expected<ProxyConfig, std::string> ProxyConfig::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue ProxyConfig::ToDict() const {
  base::DictValue dict;
  if (country.has_value()) dict.Set("country", *country);
  if (city.has_value()) dict.Set("city", *city);
  if (connection_type.has_value()) dict.Set("connection_type", *connection_type);
  if (host.has_value()) dict.Set("host", *host);
  if (port.has_value()) dict.Set("port", *port);
  if (username.has_value()) dict.Set("username", *username);
  if (password.has_value()) dict.Set("password", *password);
  return dict;
}

std::string ProxyConfig::ToJson() const {
  return DictToJson(ToDict());
}

Fingerprint::Fingerprint() = default;
Fingerprint::Fingerprint(const Fingerprint&) = default;
Fingerprint& Fingerprint::operator=(const Fingerprint&) = default;
Fingerprint::Fingerprint(Fingerprint&&) = default;
Fingerprint& Fingerprint::operator=(Fingerprint&&) = default;
Fingerprint::~Fingerprint() = default;

base::expected<Fingerprint, std::string> Fingerprint::FromDict(
    const base::DictValue& dict) {
  Fingerprint value;

  auto browser_family = RequireString(dict, "browser_family");
  if (!browser_family.has_value()) return base::unexpected(browser_family.error());
  value.browser_family = *browser_family;

  auto browser_version = RequireString(dict, "browser_version");
  if (!browser_version.has_value()) return base::unexpected(browser_version.error());
  value.browser_version = *browser_version;

  auto engine = RequireString(dict, "engine");
  if (!engine.has_value()) return base::unexpected(engine.error());
  value.engine = *engine;

  auto os = RequireString(dict, "os");
  if (!os.has_value()) return base::unexpected(os.error());
  value.os = *os;

  auto os_version = RequireString(dict, "os_version");
  if (!os_version.has_value()) return base::unexpected(os_version.error());
  value.os_version = *os_version;

  auto architecture = RequireString(dict, "architecture");
  if (!architecture.has_value()) return base::unexpected(architecture.error());
  value.architecture = *architecture;

  auto device_class = RequireString(dict, "device_class");
  if (!device_class.has_value()) return base::unexpected(device_class.error());
  value.device_class = *device_class;

  auto user_agent = RequireString(dict, "user_agent");
  if (!user_agent.has_value()) return base::unexpected(user_agent.error());
  value.user_agent = *user_agent;

  auto platform = RequireString(dict, "platform");
  if (!platform.has_value()) return base::unexpected(platform.error());
  value.platform = *platform;

  auto user_agent_data_dict = RequireDict(dict, "user_agent_data");
  if (!user_agent_data_dict.has_value()) return base::unexpected(user_agent_data_dict.error());
  auto user_agent_data = UserAgentData::FromDict(**user_agent_data_dict);
  if (!user_agent_data.has_value()) return base::unexpected(user_agent_data.error());
  value.user_agent_data = std::move(*user_agent_data);

  auto screen_dict = RequireDict(dict, "screen");
  if (!screen_dict.has_value()) return base::unexpected(screen_dict.error());
  auto screen = Screen::FromDict(**screen_dict);
  if (!screen.has_value()) return base::unexpected(screen.error());
  value.screen = std::move(*screen);

  auto hardware_dict = RequireDict(dict, "hardware");
  if (!hardware_dict.has_value()) return base::unexpected(hardware_dict.error());
  auto hardware = Hardware::FromDict(**hardware_dict);
  if (!hardware.has_value()) return base::unexpected(hardware.error());
  value.hardware = std::move(*hardware);

  auto webgl_dict = RequireDict(dict, "webgl");
  if (!webgl_dict.has_value()) return base::unexpected(webgl_dict.error());
  auto webgl = WebGL::FromDict(**webgl_dict);
  if (!webgl.has_value()) return base::unexpected(webgl.error());
  value.webgl = std::move(*webgl);

  auto canvas_seed = RequireInt(dict, "canvas_seed");
  if (!canvas_seed.has_value()) return base::unexpected(canvas_seed.error());
  value.canvas_seed = *canvas_seed;

  auto audio_seed = RequireInt(dict, "audio_seed");
  if (!audio_seed.has_value()) return base::unexpected(audio_seed.error());
  value.audio_seed = *audio_seed;

  auto client_rects_seed = RequireInt(dict, "client_rects_seed");
  if (!client_rects_seed.has_value()) return base::unexpected(client_rects_seed.error());
  value.client_rects_seed = *client_rects_seed;

  auto timezone = RequireString(dict, "timezone");
  if (!timezone.has_value()) return base::unexpected(timezone.error());
  value.timezone = *timezone;

  const base::ListValue* language_list = dict.FindList("language");
  if (!language_list) return base::unexpected("missing required array field: language");
  for (const base::Value& item : *language_list) {
    if (!item.is_string()) {
      return base::unexpected("language array contains non-string element");
    }
    value.language.push_back(item.GetString());
  }

  const base::ListValue* fonts_list = dict.FindList("fonts");
  if (!fonts_list) return base::unexpected("missing required array field: fonts");
  for (const base::Value& item : *fonts_list) {
    if (!item.is_string()) {
      return base::unexpected("fonts array contains non-string element");
    }
    value.fonts.push_back(item.GetString());
  }

  if (const base::ListValue* parsed_list = dict.FindList("media_devices")) {
    for (const base::Value& item : *parsed_list) {
      if (!item.is_dict()) {
        return base::unexpected("media_devices array contains non-object element");
      }
      auto parsed = MediaDevice::FromDict(item.GetDict());
      if (!parsed.has_value()) return base::unexpected(parsed.error());
      value.media_devices.push_back(std::move(*parsed));
    }
  }

  if (const base::ListValue* parsed_list = dict.FindList("plugins")) {
    for (const base::Value& item : *parsed_list) {
      if (!item.is_dict()) {
        return base::unexpected("plugins array contains non-object element");
      }
      auto parsed = Plugin::FromDict(item.GetDict());
      if (!parsed.has_value()) return base::unexpected(parsed.error());
      value.plugins.push_back(std::move(*parsed));
    }
  }

  if (const base::DictValue* parsed_dict = dict.FindDict("battery")) {
    auto parsed = Battery::FromDict(*parsed_dict);
    if (!parsed.has_value()) return base::unexpected(parsed.error());
    value.battery = std::move(*parsed);
  }

  if (const base::ListValue* parsed_list = dict.FindList("speech_voices")) {
    for (const base::Value& item : *parsed_list) {
      if (!item.is_string()) {
        return base::unexpected("speech_voices array contains non-string element");
      }
      value.speech_voices.push_back(item.GetString());
    }
  }

  if (const base::DictValue* parsed_dict = dict.FindDict("audio_codecs")) {
    for (auto [key, item] : *parsed_dict) {
      if (!item.is_string()) {
        return base::unexpected("audio_codecs object contains non-string value");
      }
      value.audio_codecs[key] = item.GetString();
    }
  }

  if (const base::DictValue* parsed_dict = dict.FindDict("video_codecs")) {
    for (auto [key, item] : *parsed_dict) {
      if (!item.is_string()) {
        return base::unexpected("video_codecs object contains non-string value");
      }
      value.video_codecs[key] = item.GetString();
    }
  }

  auto headers_dict = RequireDict(dict, "headers");
  if (!headers_dict.has_value()) return base::unexpected(headers_dict.error());
  for (auto [key, item] : **headers_dict) {
    if (!item.is_string()) {
      return base::unexpected("headers object contains non-string value");
    }
    value.headers[key] = item.GetString();
  }

  auto surface_policy_dict = RequireDict(dict, "surface_policy");
  if (!surface_policy_dict.has_value()) return base::unexpected(surface_policy_dict.error());
  auto surface_policy = SurfacePolicy::FromDict(**surface_policy_dict);
  if (!surface_policy.has_value()) return base::unexpected(surface_policy.error());
  value.surface_policy = std::move(*surface_policy);

  return base::ok(std::move(value));
}

base::expected<Fingerprint, std::string> Fingerprint::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue Fingerprint::ToDict() const {
  base::DictValue dict;
  dict.Set("browser_family", browser_family);
  dict.Set("browser_version", browser_version);
  dict.Set("engine", engine);
  dict.Set("os", os);
  dict.Set("os_version", os_version);
  dict.Set("architecture", architecture);
  dict.Set("device_class", device_class);
  dict.Set("user_agent", user_agent);
  dict.Set("platform", platform);
  dict.Set("user_agent_data", user_agent_data.ToDict());
  dict.Set("screen", screen.ToDict());
  dict.Set("hardware", hardware.ToDict());
  dict.Set("webgl", webgl.ToDict());
  dict.Set("canvas_seed", canvas_seed);
  dict.Set("audio_seed", audio_seed);
  dict.Set("client_rects_seed", client_rects_seed);
  dict.Set("timezone", timezone);
  {
    base::ListValue list;
    for (const auto& item : language) list.Append(item);
    dict.Set("language", std::move(list));
  }
  {
    base::ListValue list;
    for (const auto& item : fonts) list.Append(item);
    dict.Set("fonts", std::move(list));
  }
  {
    base::ListValue list;
    for (const auto& item : media_devices) list.Append(item.ToDict());
    if (!media_devices.empty()) dict.Set("media_devices", std::move(list));
  }
  {
    base::ListValue list;
    for (const auto& item : plugins) list.Append(item.ToDict());
    if (!plugins.empty()) dict.Set("plugins", std::move(list));
  }
  if (battery.has_value()) dict.Set("battery", battery->ToDict());
  {
    base::ListValue list;
    for (const auto& item : speech_voices) list.Append(item);
    if (!speech_voices.empty()) dict.Set("speech_voices", std::move(list));
  }
  {
    base::DictValue object;
    for (const auto& [key, item] : audio_codecs) object.Set(key, item);
    if (!audio_codecs.empty()) dict.Set("audio_codecs", std::move(object));
  }
  {
    base::DictValue object;
    for (const auto& [key, item] : video_codecs) object.Set(key, item);
    if (!video_codecs.empty()) dict.Set("video_codecs", std::move(object));
  }
  {
    base::DictValue object;
    for (const auto& [key, item] : headers) object.Set(key, item);
    dict.Set("headers", std::move(object));
  }
  dict.Set("surface_policy", surface_policy.ToDict());
  return dict;
}

std::string Fingerprint::ToJson() const {
  return DictToJson(ToDict());
}

GenerateResponse::GenerateResponse() = default;
GenerateResponse::GenerateResponse(const GenerateResponse&) = default;
GenerateResponse& GenerateResponse::operator=(const GenerateResponse&) = default;
GenerateResponse::GenerateResponse(GenerateResponse&&) = default;
GenerateResponse& GenerateResponse::operator=(GenerateResponse&&) = default;
GenerateResponse::~GenerateResponse() = default;

base::expected<GenerateResponse, std::string> GenerateResponse::FromDict(
    const base::DictValue& dict) {
  GenerateResponse value;

  auto fingerprint_dict = RequireDict(dict, "fingerprint");
  if (!fingerprint_dict.has_value()) return base::unexpected(fingerprint_dict.error());
  auto fingerprint = Fingerprint::FromDict(**fingerprint_dict);
  if (!fingerprint.has_value()) return base::unexpected(fingerprint.error());
  value.fingerprint = std::move(*fingerprint);

  auto generator_dict = RequireDict(dict, "generator");
  if (!generator_dict.has_value()) return base::unexpected(generator_dict.error());
  auto generator = GeneratorProvenance::FromDict(**generator_dict);
  if (!generator.has_value()) return base::unexpected(generator.error());
  value.generator = std::move(*generator);

  if (const base::DictValue* parsed_dict = dict.FindDict("proxy")) {
    auto parsed = ProxyConfig::FromDict(*parsed_dict);
    if (!parsed.has_value()) return base::unexpected(parsed.error());
    value.proxy = std::move(*parsed);
  }

  return base::ok(std::move(value));
}

base::expected<GenerateResponse, std::string> GenerateResponse::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue GenerateResponse::ToDict() const {
  base::DictValue dict;
  dict.Set("fingerprint", fingerprint.ToDict());
  dict.Set("generator", generator.ToDict());
  if (proxy.has_value()) dict.Set("proxy", proxy->ToDict());
  return dict;
}

std::string GenerateResponse::ToJson() const {
  return DictToJson(ToDict());
}

ProxyCredentials::ProxyCredentials() = default;
ProxyCredentials::ProxyCredentials(const ProxyCredentials&) = default;
ProxyCredentials& ProxyCredentials::operator=(const ProxyCredentials&) = default;
ProxyCredentials::ProxyCredentials(ProxyCredentials&&) = default;
ProxyCredentials& ProxyCredentials::operator=(ProxyCredentials&&) = default;
ProxyCredentials::~ProxyCredentials() = default;

base::expected<ProxyCredentials, std::string> ProxyCredentials::FromDict(
    const base::DictValue& dict) {
  ProxyCredentials value;

  auto host = RequireString(dict, "host");
  if (!host.has_value()) return base::unexpected(host.error());
  value.host = *host;

  auto port = RequireInt(dict, "port");
  if (!port.has_value()) return base::unexpected(port.error());
  value.port = *port;

  auto username = RequireString(dict, "username");
  if (!username.has_value()) return base::unexpected(username.error());
  value.username = *username;

  auto password = RequireString(dict, "password");
  if (!password.has_value()) return base::unexpected(password.error());
  value.password = *password;

  return base::ok(std::move(value));
}

base::expected<ProxyCredentials, std::string> ProxyCredentials::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue ProxyCredentials::ToDict() const {
  base::DictValue dict;
  dict.Set("host", host);
  dict.Set("port", port);
  dict.Set("username", username);
  dict.Set("password", password);
  return dict;
}

std::string ProxyCredentials::ToJson() const {
  return DictToJson(ToDict());
}

VerifyProxyRequest::VerifyProxyRequest() = default;
VerifyProxyRequest::VerifyProxyRequest(const VerifyProxyRequest&) = default;
VerifyProxyRequest& VerifyProxyRequest::operator=(const VerifyProxyRequest&) = default;
VerifyProxyRequest::VerifyProxyRequest(VerifyProxyRequest&&) = default;
VerifyProxyRequest& VerifyProxyRequest::operator=(VerifyProxyRequest&&) = default;
VerifyProxyRequest::~VerifyProxyRequest() = default;

base::expected<VerifyProxyRequest, std::string> VerifyProxyRequest::FromDict(
    const base::DictValue& dict) {
  VerifyProxyRequest value;

  auto proxy_dict = RequireDict(dict, "proxy");
  if (!proxy_dict.has_value()) return base::unexpected(proxy_dict.error());
  auto proxy = ProxyCredentials::FromDict(**proxy_dict);
  if (!proxy.has_value()) return base::unexpected(proxy.error());
  value.proxy = std::move(*proxy);

  auto expected_country = RequireString(dict, "expected_country");
  if (!expected_country.has_value()) return base::unexpected(expected_country.error());
  value.expected_country = *expected_country;

  if (const std::string* parsed = dict.FindString("expected_city")) value.expected_city = *parsed;

  return base::ok(std::move(value));
}

base::expected<VerifyProxyRequest, std::string> VerifyProxyRequest::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue VerifyProxyRequest::ToDict() const {
  base::DictValue dict;
  dict.Set("proxy", proxy.ToDict());
  dict.Set("expected_country", expected_country);
  if (expected_city.has_value()) dict.Set("expected_city", *expected_city);
  return dict;
}

std::string VerifyProxyRequest::ToJson() const {
  return DictToJson(ToDict());
}

VerifyProxyResponse::VerifyProxyResponse() = default;
VerifyProxyResponse::VerifyProxyResponse(const VerifyProxyResponse&) = default;
VerifyProxyResponse& VerifyProxyResponse::operator=(const VerifyProxyResponse&) = default;
VerifyProxyResponse::VerifyProxyResponse(VerifyProxyResponse&&) = default;
VerifyProxyResponse& VerifyProxyResponse::operator=(VerifyProxyResponse&&) = default;
VerifyProxyResponse::~VerifyProxyResponse() = default;

base::expected<VerifyProxyResponse, std::string> VerifyProxyResponse::FromDict(
    const base::DictValue& dict) {
  VerifyProxyResponse value;

  auto match = RequireBool(dict, "match");
  if (!match.has_value()) return base::unexpected(match.error());
  value.match = *match;

  auto actual_country = RequireString(dict, "actual_country");
  if (!actual_country.has_value()) return base::unexpected(actual_country.error());
  value.actual_country = *actual_country;

  if (const std::string* parsed = dict.FindString("actual_city")) value.actual_city = *parsed;

  if (const std::string* parsed = dict.FindString("ipv4")) value.ipv4 = *parsed;

  if (const std::string* parsed = dict.FindString("ipv6")) value.ipv6 = *parsed;

  return base::ok(std::move(value));
}

base::expected<VerifyProxyResponse, std::string> VerifyProxyResponse::FromJson(
    const std::string& json) {
  auto dict = ParseJsonToDict(json);
  if (!dict.has_value()) return base::unexpected(dict.error());
  return FromDict(*dict);
}

base::DictValue VerifyProxyResponse::ToDict() const {
  base::DictValue dict;
  dict.Set("match", match);
  dict.Set("actual_country", actual_country);
  if (actual_city.has_value()) dict.Set("actual_city", *actual_city);
  if (ipv4.has_value()) dict.Set("ipv4", *ipv4);
  if (ipv6.has_value()) dict.Set("ipv6", *ipv6);
  return dict;
}

std::string VerifyProxyResponse::ToJson() const {
  return DictToJson(ToDict());
}

}  // namespace clawbrowser
