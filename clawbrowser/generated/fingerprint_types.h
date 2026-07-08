// Copyright 2026 The Clawbrowser Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AUTO-GENERATED FILE — DO NOT EDIT MANUALLY.
// Source: clawbrowser/schemas/browser_schema.json
// See clawbrowser/generated/README.md for re-generation instructions.

#ifndef CLAWBROWSER_GENERATED_FINGERPRINT_TYPES_H_
#define CLAWBROWSER_GENERATED_FINGERPRINT_TYPES_H_

#include <optional>
#include <map>
#include <string>
#include <vector>

#include "base/types/expected.h"
#include "base/values.h"

namespace clawbrowser {

struct GenerateRequest {
  GenerateRequest();
  GenerateRequest(const GenerateRequest&);
  GenerateRequest& operator=(const GenerateRequest&);
  GenerateRequest(GenerateRequest&&);
  GenerateRequest& operator=(GenerateRequest&&);
  ~GenerateRequest();

  std::string platform;
  std::string browser;
  std::string country;
  std::optional<std::string> city;
  std::optional<std::string> connection_type;
  std::optional<std::string> proxy_scheme;
  std::optional<std::string> runtime_browser_version;
  std::optional<std::string> runtime_os;
  std::optional<std::string> runtime_os_version;
  std::optional<std::string> runtime_arch;
  std::optional<std::string> runtime_gpu;
  std::optional<bool> runtime_headless;

  static base::expected<GenerateRequest, std::string> FromDict(
      const base::DictValue& dict);
  static base::expected<GenerateRequest, std::string> FromJson(
      const std::string& json);
  base::DictValue ToDict() const;
  std::string ToJson() const;
};

struct Screen {
  int width = 0;
  int height = 0;
  int avail_width = 0;
  int avail_height = 0;
  int color_depth = 0;
  double pixel_ratio = 0.0;

  static base::expected<Screen, std::string> FromDict(
      const base::DictValue& dict);
  static base::expected<Screen, std::string> FromJson(
      const std::string& json);
  base::DictValue ToDict() const;
  std::string ToJson() const;
};

struct Hardware {
  int concurrency = 0;
  int memory = 0;

  static base::expected<Hardware, std::string> FromDict(
      const base::DictValue& dict);
  static base::expected<Hardware, std::string> FromJson(
      const std::string& json);
  base::DictValue ToDict() const;
  std::string ToJson() const;
};

struct WebGL {
  WebGL();
  WebGL(const WebGL&);
  WebGL& operator=(const WebGL&);
  WebGL(WebGL&&);
  WebGL& operator=(WebGL&&);
  ~WebGL();

  std::string vendor;
  std::string renderer;

  static base::expected<WebGL, std::string> FromDict(
      const base::DictValue& dict);
  static base::expected<WebGL, std::string> FromJson(
      const std::string& json);
  base::DictValue ToDict() const;
  std::string ToJson() const;
};

struct MediaDevice {
  MediaDevice();
  MediaDevice(const MediaDevice&);
  MediaDevice& operator=(const MediaDevice&);
  MediaDevice(MediaDevice&&);
  MediaDevice& operator=(MediaDevice&&);
  ~MediaDevice();

  std::optional<std::string> kind;
  std::optional<std::string> label;
  std::optional<std::string> device_id;

  static base::expected<MediaDevice, std::string> FromDict(
      const base::DictValue& dict);
  static base::expected<MediaDevice, std::string> FromJson(
      const std::string& json);
  base::DictValue ToDict() const;
  std::string ToJson() const;
};

struct Plugin {
  Plugin();
  Plugin(const Plugin&);
  Plugin& operator=(const Plugin&);
  Plugin(Plugin&&);
  Plugin& operator=(Plugin&&);
  ~Plugin();

  std::optional<std::string> name;
  std::optional<std::string> description;
  std::optional<std::string> filename;

  static base::expected<Plugin, std::string> FromDict(
      const base::DictValue& dict);
  static base::expected<Plugin, std::string> FromJson(
      const std::string& json);
  base::DictValue ToDict() const;
  std::string ToJson() const;
};

struct Battery {
  Battery();
  Battery(const Battery&);
  Battery& operator=(const Battery&);
  Battery(Battery&&);
  Battery& operator=(Battery&&);
  ~Battery();

  std::optional<bool> charging;
  std::optional<double> level;

  static base::expected<Battery, std::string> FromDict(
      const base::DictValue& dict);
  static base::expected<Battery, std::string> FromJson(
      const std::string& json);
  base::DictValue ToDict() const;
  std::string ToJson() const;
};

struct ClientHintBrand {
  ClientHintBrand();
  ClientHintBrand(const ClientHintBrand&);
  ClientHintBrand& operator=(const ClientHintBrand&);
  ClientHintBrand(ClientHintBrand&&);
  ClientHintBrand& operator=(ClientHintBrand&&);
  ~ClientHintBrand();

  std::string brand;
  std::string version;

  static base::expected<ClientHintBrand, std::string> FromDict(
      const base::DictValue& dict);
  static base::expected<ClientHintBrand, std::string> FromJson(
      const std::string& json);
  base::DictValue ToDict() const;
  std::string ToJson() const;
};

struct UserAgentData {
  UserAgentData();
  UserAgentData(const UserAgentData&);
  UserAgentData& operator=(const UserAgentData&);
  UserAgentData(UserAgentData&&);
  UserAgentData& operator=(UserAgentData&&);
  ~UserAgentData();

  std::vector<ClientHintBrand> brands;
  std::vector<ClientHintBrand> fullVersionList;
  std::string platform;
  std::string platformVersion;
  std::string architecture;
  std::string bitness;
  bool mobile = false;
  std::string model;
  std::optional<std::string> uaFullVersion;

  static base::expected<UserAgentData, std::string> FromDict(
      const base::DictValue& dict);
  static base::expected<UserAgentData, std::string> FromJson(
      const std::string& json);
  base::DictValue ToDict() const;
  std::string ToJson() const;
};

struct SurfacePolicyRule {
  SurfacePolicyRule();
  SurfacePolicyRule(const SurfacePolicyRule&);
  SurfacePolicyRule& operator=(const SurfacePolicyRule&);
  SurfacePolicyRule(SurfacePolicyRule&&);
  SurfacePolicyRule& operator=(SurfacePolicyRule&&);
  ~SurfacePolicyRule();

  std::string mode;

  static base::expected<SurfacePolicyRule, std::string> FromDict(
      const base::DictValue& dict);
  static base::expected<SurfacePolicyRule, std::string> FromJson(
      const std::string& json);
  base::DictValue ToDict() const;
  std::string ToJson() const;
};

struct SurfacePolicy {
  SurfacePolicy();
  SurfacePolicy(const SurfacePolicy&);
  SurfacePolicy& operator=(const SurfacePolicy&);
  SurfacePolicy(SurfacePolicy&&);
  SurfacePolicy& operator=(SurfacePolicy&&);
  ~SurfacePolicy();

  SurfacePolicyRule canvas;
  SurfacePolicyRule audio;
  SurfacePolicyRule client_rects;
  SurfacePolicyRule webgl;
  SurfacePolicyRule fonts;
  SurfacePolicyRule plugins;
  SurfacePolicyRule media_devices;
  SurfacePolicyRule speech_voices;

  static base::expected<SurfacePolicy, std::string> FromDict(
      const base::DictValue& dict);
  static base::expected<SurfacePolicy, std::string> FromJson(
      const std::string& json);
  base::DictValue ToDict() const;
  std::string ToJson() const;
};

struct ProxyConfig {
  ProxyConfig();
  ProxyConfig(const ProxyConfig&);
  ProxyConfig& operator=(const ProxyConfig&);
  ProxyConfig(ProxyConfig&&);
  ProxyConfig& operator=(ProxyConfig&&);
  ~ProxyConfig();

  std::optional<std::string> scheme;
  std::optional<std::string> country;
  std::optional<std::string> city;
  std::optional<std::string> connection_type;
  std::optional<std::string> host;
  std::optional<int> port;
  std::optional<std::string> username;
  std::optional<std::string> password;

  static base::expected<ProxyConfig, std::string> FromDict(
      const base::DictValue& dict);
  static base::expected<ProxyConfig, std::string> FromJson(
      const std::string& json);
  base::DictValue ToDict() const;
  std::string ToJson() const;
};

struct Fingerprint {
  Fingerprint();
  Fingerprint(const Fingerprint&);
  Fingerprint& operator=(const Fingerprint&);
  Fingerprint(Fingerprint&&);
  Fingerprint& operator=(Fingerprint&&);
  ~Fingerprint();

  std::string browser_family;
  std::string browser_version;
  std::string engine;
  std::string os;
  std::string os_version;
  std::string architecture;
  std::string device_class;
  std::string user_agent;
  std::string platform;
  UserAgentData user_agent_data;
  Screen screen;
  Hardware hardware;
  WebGL webgl;
  int canvas_seed = 0;
  int audio_seed = 0;
  int client_rects_seed = 0;
  std::string timezone;
  std::vector<std::string> language;
  std::vector<std::string> fonts;
  std::vector<MediaDevice> media_devices;
  std::vector<Plugin> plugins;
  std::optional<Battery> battery;
  std::vector<std::string> speech_voices;
  std::map<std::string, std::string> audio_codecs;
  std::map<std::string, std::string> video_codecs;
  std::map<std::string, std::string> headers;
  SurfacePolicy surface_policy;

  static base::expected<Fingerprint, std::string> FromDict(
      const base::DictValue& dict);
  static base::expected<Fingerprint, std::string> FromJson(
      const std::string& json);
  base::DictValue ToDict() const;
  std::string ToJson() const;
};

struct GenerateResponse {
  GenerateResponse();
  GenerateResponse(const GenerateResponse&);
  GenerateResponse& operator=(const GenerateResponse&);
  GenerateResponse(GenerateResponse&&);
  GenerateResponse& operator=(GenerateResponse&&);
  ~GenerateResponse();

  Fingerprint fingerprint;
  std::optional<ProxyConfig> proxy;

  static base::expected<GenerateResponse, std::string> FromDict(
      const base::DictValue& dict);
  static base::expected<GenerateResponse, std::string> FromJson(
      const std::string& json);
  base::DictValue ToDict() const;
  std::string ToJson() const;
};

struct ProxyCredentials {
  ProxyCredentials();
  ProxyCredentials(const ProxyCredentials&);
  ProxyCredentials& operator=(const ProxyCredentials&);
  ProxyCredentials(ProxyCredentials&&);
  ProxyCredentials& operator=(ProxyCredentials&&);
  ~ProxyCredentials();

  std::optional<std::string> scheme;
  std::string host;
  int port = 0;
  std::string username;
  std::string password;

  static base::expected<ProxyCredentials, std::string> FromDict(
      const base::DictValue& dict);
  static base::expected<ProxyCredentials, std::string> FromJson(
      const std::string& json);
  base::DictValue ToDict() const;
  std::string ToJson() const;
};

struct VerifyProxyRequest {
  VerifyProxyRequest();
  VerifyProxyRequest(const VerifyProxyRequest&);
  VerifyProxyRequest& operator=(const VerifyProxyRequest&);
  VerifyProxyRequest(VerifyProxyRequest&&);
  VerifyProxyRequest& operator=(VerifyProxyRequest&&);
  ~VerifyProxyRequest();

  ProxyCredentials proxy;
  std::string expected_country;
  std::optional<std::string> expected_city;

  static base::expected<VerifyProxyRequest, std::string> FromDict(
      const base::DictValue& dict);
  static base::expected<VerifyProxyRequest, std::string> FromJson(
      const std::string& json);
  base::DictValue ToDict() const;
  std::string ToJson() const;
};

struct VerifyProxyResponse {
  VerifyProxyResponse();
  VerifyProxyResponse(const VerifyProxyResponse&);
  VerifyProxyResponse& operator=(const VerifyProxyResponse&);
  VerifyProxyResponse(VerifyProxyResponse&&);
  VerifyProxyResponse& operator=(VerifyProxyResponse&&);
  ~VerifyProxyResponse();

  bool match = false;
  std::string actual_country;
  std::optional<std::string> actual_city;
  std::optional<std::string> ipv4;
  std::optional<std::string> ipv6;

  static base::expected<VerifyProxyResponse, std::string> FromDict(
      const base::DictValue& dict);
  static base::expected<VerifyProxyResponse, std::string> FromJson(
      const std::string& json);
  base::DictValue ToDict() const;
  std::string ToJson() const;
};

}  // namespace clawbrowser

#endif  // CLAWBROWSER_GENERATED_FINGERPRINT_TYPES_H_
