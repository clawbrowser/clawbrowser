#ifndef CLAWBROWSER_PROXY_PROXY_CONFIG_H_
#define CLAWBROWSER_PROXY_PROXY_CONFIG_H_

#include <optional>
#include <string>
#include <vector>

#include "clawbrowser/fingerprint_accessor.h"

namespace clawbrowser {

struct ClawbrowserProxyConfig {
  std::string proxy_server;  // <scheme>://host:port
  std::string username;
  std::string password;
};

// Build proxy config from fingerprint proxy data.
// Returns nullopt if no proxy provided.
std::optional<ClawbrowserProxyConfig> BuildClawbrowserProxyConfig(
    const std::optional<RuntimeProxyConfig>& proxy);

// Get command-line flags for proxy configuration.
std::vector<std::string> GetProxyCommandLineFlags(
    const RuntimeProxyConfig& proxy);

}  // namespace clawbrowser

#endif  // CLAWBROWSER_PROXY_PROXY_CONFIG_H_
