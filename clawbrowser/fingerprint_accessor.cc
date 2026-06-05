#include "clawbrowser/fingerprint_accessor.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"

namespace clawbrowser {

namespace {

struct RuntimeState {
  RuntimeFingerprint fingerprint;
  std::optional<RuntimeProxyConfig> proxy;
};

std::unique_ptr<RuntimeState>& RuntimeStateStorage() {
  static base::NoDestructor<std::unique_ptr<RuntimeState>> state;
  return *state;
}

}  // namespace

RuntimeWebGL::RuntimeWebGL() = default;
RuntimeWebGL::RuntimeWebGL(const RuntimeWebGL&) = default;
RuntimeWebGL& RuntimeWebGL::operator=(const RuntimeWebGL&) = default;
RuntimeWebGL::RuntimeWebGL(RuntimeWebGL&&) = default;
RuntimeWebGL& RuntimeWebGL::operator=(RuntimeWebGL&&) = default;
RuntimeWebGL::~RuntimeWebGL() = default;

RuntimeMediaDevice::RuntimeMediaDevice() = default;
RuntimeMediaDevice::RuntimeMediaDevice(const RuntimeMediaDevice&) = default;
RuntimeMediaDevice& RuntimeMediaDevice::operator=(const RuntimeMediaDevice&) =
    default;
RuntimeMediaDevice::RuntimeMediaDevice(RuntimeMediaDevice&&) = default;
RuntimeMediaDevice& RuntimeMediaDevice::operator=(RuntimeMediaDevice&&) =
    default;
RuntimeMediaDevice::~RuntimeMediaDevice() = default;

RuntimePlugin::RuntimePlugin() = default;
RuntimePlugin::RuntimePlugin(const RuntimePlugin&) = default;
RuntimePlugin& RuntimePlugin::operator=(const RuntimePlugin&) = default;
RuntimePlugin::RuntimePlugin(RuntimePlugin&&) = default;
RuntimePlugin& RuntimePlugin::operator=(RuntimePlugin&&) = default;
RuntimePlugin::~RuntimePlugin() = default;

RuntimeBattery::RuntimeBattery() = default;
RuntimeBattery::RuntimeBattery(const RuntimeBattery&) = default;
RuntimeBattery& RuntimeBattery::operator=(const RuntimeBattery&) = default;
RuntimeBattery::RuntimeBattery(RuntimeBattery&&) = default;
RuntimeBattery& RuntimeBattery::operator=(RuntimeBattery&&) = default;
RuntimeBattery::~RuntimeBattery() = default;

RuntimeClientHintBrand::RuntimeClientHintBrand() = default;
RuntimeClientHintBrand::RuntimeClientHintBrand(
    const RuntimeClientHintBrand&) = default;
RuntimeClientHintBrand& RuntimeClientHintBrand::operator=(
    const RuntimeClientHintBrand&) = default;
RuntimeClientHintBrand::RuntimeClientHintBrand(
    RuntimeClientHintBrand&&) = default;
RuntimeClientHintBrand& RuntimeClientHintBrand::operator=(
    RuntimeClientHintBrand&&) = default;
RuntimeClientHintBrand::~RuntimeClientHintBrand() = default;

RuntimeUserAgentData::RuntimeUserAgentData() = default;
RuntimeUserAgentData::RuntimeUserAgentData(
    const RuntimeUserAgentData&) = default;
RuntimeUserAgentData& RuntimeUserAgentData::operator=(
    const RuntimeUserAgentData&) = default;
RuntimeUserAgentData::RuntimeUserAgentData(RuntimeUserAgentData&&) = default;
RuntimeUserAgentData& RuntimeUserAgentData::operator=(
    RuntimeUserAgentData&&) = default;
RuntimeUserAgentData::~RuntimeUserAgentData() = default;

RuntimeSurfacePolicy::RuntimeSurfacePolicy() = default;
RuntimeSurfacePolicy::RuntimeSurfacePolicy(
    const RuntimeSurfacePolicy&) = default;
RuntimeSurfacePolicy& RuntimeSurfacePolicy::operator=(
    const RuntimeSurfacePolicy&) = default;
RuntimeSurfacePolicy::RuntimeSurfacePolicy(RuntimeSurfacePolicy&&) = default;
RuntimeSurfacePolicy& RuntimeSurfacePolicy::operator=(
    RuntimeSurfacePolicy&&) = default;
RuntimeSurfacePolicy::~RuntimeSurfacePolicy() = default;

RuntimeProxyConfig::RuntimeProxyConfig() = default;
RuntimeProxyConfig::RuntimeProxyConfig(const RuntimeProxyConfig&) = default;
RuntimeProxyConfig& RuntimeProxyConfig::operator=(const RuntimeProxyConfig&) =
    default;
RuntimeProxyConfig::RuntimeProxyConfig(RuntimeProxyConfig&&) = default;
RuntimeProxyConfig& RuntimeProxyConfig::operator=(RuntimeProxyConfig&&) =
    default;
RuntimeProxyConfig::~RuntimeProxyConfig() = default;

RuntimeFingerprint::RuntimeFingerprint() = default;
RuntimeFingerprint::RuntimeFingerprint(const RuntimeFingerprint&) = default;
RuntimeFingerprint& RuntimeFingerprint::operator=(const RuntimeFingerprint&) =
    default;
RuntimeFingerprint::RuntimeFingerprint(RuntimeFingerprint&&) = default;
RuntimeFingerprint& RuntimeFingerprint::operator=(RuntimeFingerprint&&) =
    default;
RuntimeFingerprint::~RuntimeFingerprint() = default;

// static
const RuntimeFingerprint* FingerprintAccessor::Get() {
  const auto& state = RuntimeStateStorage();
  return state ? &state->fingerprint : nullptr;
}

// static
const RuntimeProxyConfig* FingerprintAccessor::GetProxy() {
  const auto& state = RuntimeStateStorage();
  if (!state || !state->proxy)
    return nullptr;
  return &(*state->proxy);
}

// static
void FingerprintAccessor::Set(RuntimeFingerprint fingerprint,
                              std::optional<RuntimeProxyConfig> proxy) {
  RuntimeStateStorage() = std::make_unique<RuntimeState>(
      RuntimeState{std::move(fingerprint), std::move(proxy)});
}

// static
void FingerprintAccessor::Reset() {
  RuntimeStateStorage().reset();
}

}  // namespace clawbrowser
