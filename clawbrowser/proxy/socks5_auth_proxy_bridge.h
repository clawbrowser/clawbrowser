#ifndef CLAWBROWSER_PROXY_SOCKS5_AUTH_PROXY_BRIDGE_H_
#define CLAWBROWSER_PROXY_SOCKS5_AUTH_PROXY_BRIDGE_H_

#include <memory>
#include <string>

#include "base/types/expected.h"
#include "clawbrowser/fingerprint_accessor.h"
#include "clawbrowser/proxy/proxy_config.h"

namespace clawbrowser {

class Socks5AuthProxyBridge {
 public:
  static base::expected<std::unique_ptr<Socks5AuthProxyBridge>, std::string>
  Start(const RuntimeProxyConfig& proxy);

  Socks5AuthProxyBridge(const Socks5AuthProxyBridge&) = delete;
  Socks5AuthProxyBridge& operator=(const Socks5AuthProxyBridge&) = delete;
  ~Socks5AuthProxyBridge();

  ProxyBridgeEndpoint endpoint() const;

 private:
  class Impl;

  explicit Socks5AuthProxyBridge(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

}  // namespace clawbrowser

#endif  // CLAWBROWSER_PROXY_SOCKS5_AUTH_PROXY_BRIDGE_H_
