#ifndef CLAWBROWSER_FINGERPRINT_ACCESSOR_H_
#define CLAWBROWSER_FINGERPRINT_ACCESSOR_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"

namespace clawbrowser {

struct RuntimeScreen {
  int width = 0;
  int height = 0;
  int avail_width = 0;
  int avail_height = 0;
  int color_depth = 0;
  double pixel_ratio = 0.0;
};

struct RuntimeHardware {
  int concurrency = 0;
  int memory = 0;
};

struct COMPONENT_EXPORT(CLAWBROWSER_RUNTIME) RuntimeWebGL {
  RuntimeWebGL();
  RuntimeWebGL(const RuntimeWebGL&);
  RuntimeWebGL& operator=(const RuntimeWebGL&);
  RuntimeWebGL(RuntimeWebGL&&);
  RuntimeWebGL& operator=(RuntimeWebGL&&);
  ~RuntimeWebGL();

  std::string vendor;
  std::string renderer;
};

struct COMPONENT_EXPORT(CLAWBROWSER_RUNTIME) RuntimeMediaDevice {
  RuntimeMediaDevice();
  RuntimeMediaDevice(const RuntimeMediaDevice&);
  RuntimeMediaDevice& operator=(const RuntimeMediaDevice&);
  RuntimeMediaDevice(RuntimeMediaDevice&&);
  RuntimeMediaDevice& operator=(RuntimeMediaDevice&&);
  ~RuntimeMediaDevice();

  std::optional<std::string> kind;
  std::optional<std::string> label;
  std::optional<std::string> device_id;
};

struct COMPONENT_EXPORT(CLAWBROWSER_RUNTIME) RuntimePlugin {
  RuntimePlugin();
  RuntimePlugin(const RuntimePlugin&);
  RuntimePlugin& operator=(const RuntimePlugin&);
  RuntimePlugin(RuntimePlugin&&);
  RuntimePlugin& operator=(RuntimePlugin&&);
  ~RuntimePlugin();

  std::optional<std::string> name;
  std::optional<std::string> description;
  std::optional<std::string> filename;
};

struct COMPONENT_EXPORT(CLAWBROWSER_RUNTIME) RuntimeBattery {
  RuntimeBattery();
  RuntimeBattery(const RuntimeBattery&);
  RuntimeBattery& operator=(const RuntimeBattery&);
  RuntimeBattery(RuntimeBattery&&);
  RuntimeBattery& operator=(RuntimeBattery&&);
  ~RuntimeBattery();

  std::optional<bool> charging;
  std::optional<double> level;
};

struct COMPONENT_EXPORT(CLAWBROWSER_RUNTIME) RuntimeClientHintBrand {
  RuntimeClientHintBrand();
  RuntimeClientHintBrand(const RuntimeClientHintBrand&);
  RuntimeClientHintBrand& operator=(const RuntimeClientHintBrand&);
  RuntimeClientHintBrand(RuntimeClientHintBrand&&);
  RuntimeClientHintBrand& operator=(RuntimeClientHintBrand&&);
  ~RuntimeClientHintBrand();

  std::string brand;
  std::string version;
};

struct COMPONENT_EXPORT(CLAWBROWSER_RUNTIME) RuntimeUserAgentData {
  RuntimeUserAgentData();
  RuntimeUserAgentData(const RuntimeUserAgentData&);
  RuntimeUserAgentData& operator=(const RuntimeUserAgentData&);
  RuntimeUserAgentData(RuntimeUserAgentData&&);
  RuntimeUserAgentData& operator=(RuntimeUserAgentData&&);
  ~RuntimeUserAgentData();

  std::vector<RuntimeClientHintBrand> brands;
  std::vector<RuntimeClientHintBrand> full_version_list;
  std::string platform;
  std::string platform_version;
  std::string architecture;
  std::string bitness;
  bool mobile = false;
  std::string model;
  std::optional<std::string> ua_full_version;
};

struct COMPONENT_EXPORT(CLAWBROWSER_RUNTIME) RuntimeSurfacePolicy {
  RuntimeSurfacePolicy();
  RuntimeSurfacePolicy(const RuntimeSurfacePolicy&);
  RuntimeSurfacePolicy& operator=(const RuntimeSurfacePolicy&);
  RuntimeSurfacePolicy(RuntimeSurfacePolicy&&);
  RuntimeSurfacePolicy& operator=(RuntimeSurfacePolicy&&);
  ~RuntimeSurfacePolicy();

  std::string canvas;
  std::string audio;
  std::string client_rects;
  std::string webgl;
  std::string fonts;
  std::string plugins;
  std::string media_devices;
  std::string speech_voices;
};

struct COMPONENT_EXPORT(CLAWBROWSER_RUNTIME) RuntimeProxyConfig {
  RuntimeProxyConfig();
  RuntimeProxyConfig(const RuntimeProxyConfig&);
  RuntimeProxyConfig& operator=(const RuntimeProxyConfig&);
  RuntimeProxyConfig(RuntimeProxyConfig&&);
  RuntimeProxyConfig& operator=(RuntimeProxyConfig&&);
  ~RuntimeProxyConfig();

  std::optional<std::string> scheme;
  std::optional<std::string> country;
  std::optional<std::string> city;
  std::optional<std::string> connection_type;
  std::optional<std::string> host;
  std::optional<int> port;
  std::optional<std::string> username;
  std::optional<std::string> password;
};

struct COMPONENT_EXPORT(CLAWBROWSER_RUNTIME) RuntimeFingerprint {
  RuntimeFingerprint();
  RuntimeFingerprint(const RuntimeFingerprint&);
  RuntimeFingerprint& operator=(const RuntimeFingerprint&);
  RuntimeFingerprint(RuntimeFingerprint&&);
  RuntimeFingerprint& operator=(RuntimeFingerprint&&);
  ~RuntimeFingerprint();

  std::string browser_family;
  std::string browser_version;
  std::string engine;
  std::string os;
  std::string os_version;
  std::string architecture;
  std::string device_class;
  std::string user_agent;
  std::string platform;
  RuntimeUserAgentData user_agent_data;
  RuntimeScreen screen;
  RuntimeHardware hardware;
  RuntimeWebGL webgl;
  bool webgl_spoofing_enabled = false;
  int canvas_seed = 0;
  bool canvas_spoofing_enabled = false;
  int audio_seed = 0;
  int client_rects_seed = 0;
  std::string timezone;
  std::vector<std::string> language;
  std::vector<std::string> fonts;
  std::vector<RuntimeMediaDevice> media_devices;
  std::vector<RuntimePlugin> plugins;
  std::optional<RuntimeBattery> battery;
  std::vector<std::string> speech_voices;
  std::map<std::string, std::string> audio_codecs;
  std::map<std::string, std::string> video_codecs;
  std::map<std::string, std::string> headers;
  RuntimeSurfacePolicy surface_policy;
};

// Process-global singleton providing read-only access to the loaded runtime
// fingerprint. Returns nullptr if no fingerprint is loaded (vanilla mode).
class COMPONENT_EXPORT(CLAWBROWSER_RUNTIME) FingerprintAccessor {
 public:
  // Returns the loaded fingerprint, or nullptr if not loaded.
  static const RuntimeFingerprint* Get();

  // Returns the loaded proxy config, or nullptr.
  static const RuntimeProxyConfig* GetProxy();

  // Replace the currently loaded runtime fingerprint state.
  static void Set(RuntimeFingerprint fingerprint,
                  std::optional<RuntimeProxyConfig> proxy = std::nullopt);

  static void SetSpoofingPolicy(bool canvas_enabled, bool webgl_enabled);

  // Clear loaded data. For testing only.
  static void Reset();

 private:
  FingerprintAccessor() = delete;
};

}  // namespace clawbrowser

#endif  // CLAWBROWSER_FINGERPRINT_ACCESSOR_H_
