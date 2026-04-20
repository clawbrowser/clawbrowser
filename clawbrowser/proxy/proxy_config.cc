#include "clawbrowser/proxy/proxy_config.h"

#include "base/strings/string_number_conversions.h"

namespace clawbrowser {

namespace {

bool HasProxyEndpoint(const RuntimeProxyConfig& proxy) {
  return proxy.host.has_value() && !proxy.host->empty() && proxy.port.has_value();
}

std::optional<std::string> NormalizedProxyScheme(
    const RuntimeProxyConfig& proxy) {
  const std::string scheme =
      proxy.scheme.has_value() && !proxy.scheme->empty() ? *proxy.scheme
                                                         : "http";
  if (scheme == "http" || scheme == "socks5") {
    return scheme;
  }
  return std::nullopt;
}

std::optional<std::string> FormatProxyServer(const RuntimeProxyConfig& proxy) {
  std::optional<std::string> scheme = NormalizedProxyScheme(proxy);
  if (!scheme.has_value()) {
    return std::nullopt;
  }

  return *scheme + "://" + *proxy.host + ":" +
         base::NumberToString(*proxy.port);
}

}  // namespace

std::optional<ClawbrowserProxyConfig> BuildClawbrowserProxyConfig(
    const std::optional<RuntimeProxyConfig>& proxy) {
  if (!proxy || !HasProxyEndpoint(*proxy))
    return std::nullopt;

  std::optional<std::string> proxy_server = FormatProxyServer(*proxy);
  if (!proxy_server.has_value()) {
    return std::nullopt;
  }

  ClawbrowserProxyConfig config;
  config.proxy_server = *proxy_server;
  if (proxy->username.has_value())
    config.username = *proxy->username;
  if (proxy->password.has_value())
    config.password = *proxy->password;
  return config;
}

std::vector<std::string> GetProxyCommandLineFlags(
    const RuntimeProxyConfig& proxy) {
  if (!HasProxyEndpoint(proxy))
    return {};

  std::optional<std::string> proxy_server = FormatProxyServer(proxy);
  if (!proxy_server.has_value()) {
    return {};
  }

  std::vector<std::string> flags;
  flags.push_back("--proxy-server=" + *proxy_server);
  return flags;
}

}  // namespace clawbrowser
