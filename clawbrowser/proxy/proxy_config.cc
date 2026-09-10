#include "clawbrowser/proxy/proxy_config.h"

#include <string_view>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace clawbrowser {

namespace {

// A fingerprint-backed managed session can proxy normal browser traffic while
// WebRTC independently opens a direct UDP/STUN route. Headful Chrome reads
// this switch into the webrtc.ip_handling_policy preference. The similarly
// named --force-webrtc-ip-handling-policy switch is only consumed by Content
// Shell and Headless, so retain both forms: the headful switch protects the
// managed production Clawbrowser path and the force switch protects its
// test/headless entry points.
constexpr char kWebRtcIpHandlingPolicy[] =
    "--webrtc-ip-handling-policy=disable_non_proxied_udp";
constexpr char kForceWebRtcIpHandlingPolicy[] =
    "--force-webrtc-ip-handling-policy=disable_non_proxied_udp";

bool HasProxyEndpoint(const RuntimeProxyConfig& proxy) {
  return proxy.host.has_value() && !proxy.host->empty() &&
         proxy.port.has_value();
}

bool HasCompleteProxyCredentials(const RuntimeProxyConfig& proxy) {
  return proxy.username.has_value() && !proxy.username->empty() &&
         proxy.password.has_value() && !proxy.password->empty();
}

std::optional<std::string> NormalizedProxyScheme(
    const RuntimeProxyConfig& proxy) {
  const std::string scheme =
      base::ToLowerASCII(proxy.scheme.has_value() && !proxy.scheme->empty()
                             ? *proxy.scheme
                             : "http");
  if (scheme == "http" || scheme == "socks5") {
    return scheme;
  }
  return std::nullopt;
}

base::expected<void, std::string> AssignProxyUrlCredentials(
    std::string_view userinfo,
    RuntimeProxyConfig* proxy) {
  size_t separator = userinfo.find(':');
  if (separator == std::string_view::npos || separator == 0 ||
      separator == userinfo.size() - 1) {
    return base::unexpected(
        "proxy URL credentials must include username and password");
  }

  proxy->username = std::string(userinfo.substr(0, separator));
  proxy->password = std::string(userinfo.substr(separator + 1));
  return base::ok();
}

base::expected<void, std::string> AssignProxyUrlHostPort(
    std::string_view authority,
    RuntimeProxyConfig* proxy) {
  if (authority.empty()) {
    return base::unexpected("proxy URL missing host and port");
  }

  std::string_view host;
  std::string_view port_text;
  if (authority.front() == '[') {
    size_t close = authority.find(']');
    if (close == std::string_view::npos || close + 2 > authority.size() ||
        authority[close + 1] != ':') {
      return base::unexpected("proxy URL has invalid IPv6 host/port");
    }
    host = authority.substr(1, close - 1);
    port_text = authority.substr(close + 2);
  } else {
    size_t separator = authority.rfind(':');
    if (separator == std::string_view::npos || separator == 0 ||
        separator == authority.size() - 1) {
      return base::unexpected("proxy URL must include host and port");
    }
    host = authority.substr(0, separator);
    port_text = authority.substr(separator + 1);
  }

  int port = 0;
  if (!base::StringToInt(std::string(port_text), &port) || port <= 0 ||
      port > 65535) {
    return base::unexpected("proxy URL has invalid port");
  }

  proxy->host = std::string(host);
  proxy->port = port;
  return base::ok();
}

std::optional<std::string> FormatProxyServer(
    const RuntimeProxyConfig& proxy,
    const ProxyBridgeEndpoint* bridge_endpoint) {
  std::optional<std::string> scheme = NormalizedProxyScheme(proxy);
  if (!scheme.has_value()) {
    return std::nullopt;
  }

  if (bridge_endpoint && ShouldUseSocks5AuthBridge(proxy)) {
    return "http://" + bridge_endpoint->host + ":" +
           base::NumberToString(bridge_endpoint->port);
  }

  return *scheme + "://" + *proxy.host + ":" +
         base::NumberToString(*proxy.port);
}

}  // namespace

base::expected<RuntimeProxyConfig, std::string> ParseProxyUrl(
    const std::string& proxy_url) {
  size_t scheme_separator = proxy_url.find("://");
  if (scheme_separator == std::string::npos || scheme_separator == 0) {
    return base::unexpected("proxy URL must include a scheme");
  }

  RuntimeProxyConfig proxy;
  proxy.scheme = base::ToLowerASCII(proxy_url.substr(0, scheme_separator));
  if (!NormalizedProxyScheme(proxy).has_value()) {
    return base::unexpected("unsupported proxy scheme: " + *proxy.scheme);
  }

  std::string_view rest =
      std::string_view(proxy_url).substr(scheme_separator + 3);
  size_t path_start = rest.find_first_of("/?#");
  std::string_view authority =
      path_start == std::string_view::npos ? rest : rest.substr(0, path_start);
  if (authority.empty()) {
    return base::unexpected("proxy URL missing authority");
  }

  size_t at = authority.rfind('@');
  if (at != std::string_view::npos) {
    auto credentials_result =
        AssignProxyUrlCredentials(authority.substr(0, at), &proxy);
    if (!credentials_result.has_value()) {
      return base::unexpected(credentials_result.error());
    }
    authority = authority.substr(at + 1);
  }

  auto endpoint_result = AssignProxyUrlHostPort(authority, &proxy);
  if (!endpoint_result.has_value()) {
    return base::unexpected(endpoint_result.error());
  }

  return base::ok(std::move(proxy));
}

bool ShouldUseSocks5AuthBridge(const RuntimeProxyConfig& proxy) {
  std::optional<std::string> scheme = NormalizedProxyScheme(proxy);
  return scheme.has_value() && *scheme == "socks5" &&
         HasProxyEndpoint(proxy) && HasCompleteProxyCredentials(proxy);
}

std::optional<ClawbrowserProxyConfig> BuildClawbrowserProxyConfig(
    const std::optional<RuntimeProxyConfig>& proxy) {
  if (!proxy || !HasProxyEndpoint(*proxy))
    return std::nullopt;

  std::optional<std::string> proxy_server = FormatProxyServer(*proxy, nullptr);
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

  std::optional<std::string> proxy_server = FormatProxyServer(proxy, nullptr);
  if (!proxy_server.has_value()) {
    return {};
  }

  std::vector<std::string> flags;
  flags.push_back("--proxy-server=" + *proxy_server);
  flags.push_back(kWebRtcIpHandlingPolicy);
  flags.push_back(kForceWebRtcIpHandlingPolicy);
  return flags;
}

std::vector<std::string> GetProxyCommandLineFlags(
    const RuntimeProxyConfig& proxy,
    const ProxyBridgeEndpoint& bridge_endpoint) {
  if (!HasProxyEndpoint(proxy))
    return {};

  std::optional<std::string> proxy_server =
      FormatProxyServer(proxy, &bridge_endpoint);
  if (!proxy_server.has_value()) {
    return {};
  }

  std::vector<std::string> flags;
  flags.push_back("--proxy-server=" + *proxy_server);
  flags.push_back(kWebRtcIpHandlingPolicy);
  flags.push_back(kForceWebRtcIpHandlingPolicy);
  return flags;
}

}  // namespace clawbrowser
