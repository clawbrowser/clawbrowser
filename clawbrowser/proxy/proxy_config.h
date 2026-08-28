#ifndef CLAWBROWSER_PROXY_PROXY_CONFIG_H_
#define CLAWBROWSER_PROXY_PROXY_CONFIG_H_

#include <optional>
#include <string>
#include <vector>

#include "base/types/expected.h"
#include "clawbrowser/fingerprint_accessor.h"

namespace clawbrowser {

struct ProxyBridgeEndpoint {
  std::string host;
  int port = 0;
};

struct ClawbrowserProxyConfig {
  std::string proxy_server;  // <scheme>://host:port
  std::string username;
  std::string password;
};

// Parse a structured proxy URL such as socks5://user:pass@host:1080.
base::expected<RuntimeProxyConfig, std::string> ParseProxyUrl(
    const std::string& proxy_url);

// Returns true when Chromium needs the local HTTP bridge for SOCKS5 auth.
bool ShouldUseSocks5AuthBridge(const RuntimeProxyConfig& proxy);

// Build proxy config from fingerprint proxy data.
// Returns nullopt if no proxy provided.
std::optional<ClawbrowserProxyConfig> BuildClawbrowserProxyConfig(
    const std::optional<RuntimeProxyConfig>& proxy);

// Get command-line flags for proxy configuration.
std::vector<std::string> GetProxyCommandLineFlags(
    const RuntimeProxyConfig& proxy);
std::vector<std::string> GetProxyCommandLineFlags(
    const RuntimeProxyConfig& proxy,
    const ProxyBridgeEndpoint& bridge_endpoint);

}  // namespace clawbrowser

#endif  // CLAWBROWSER_PROXY_PROXY_CONFIG_H_
