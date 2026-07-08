#include "clawbrowser/proxy/socks5_auth_proxy_bridge.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

RuntimeProxyConfig AuthenticatedSocks5Proxy() {
  RuntimeProxyConfig proxy;
  proxy.scheme = "socks5";
  proxy.host = "gate.nodemaven.com";
  proxy.port = 1080;
  proxy.username = "user";
  proxy.password = "pass";
  return proxy;
}

TEST(Socks5AuthProxyBridgeTest, StartsOnLoopbackEndpoint) {
  auto bridge = Socks5AuthProxyBridge::Start(AuthenticatedSocks5Proxy());
  ASSERT_TRUE(bridge.has_value()) << bridge.error();

  EXPECT_EQ((*bridge)->endpoint().host, "127.0.0.1");
  EXPECT_GT((*bridge)->endpoint().port, 0);
}

TEST(Socks5AuthProxyBridgeTest, RejectsNonSocks5Scheme) {
  RuntimeProxyConfig proxy = AuthenticatedSocks5Proxy();
  proxy.scheme = "http";

  auto bridge = Socks5AuthProxyBridge::Start(proxy);
  EXPECT_FALSE(bridge.has_value());
}

TEST(Socks5AuthProxyBridgeTest, RejectsIncompleteCredentials) {
  RuntimeProxyConfig proxy = AuthenticatedSocks5Proxy();
  proxy.password.reset();

  auto bridge = Socks5AuthProxyBridge::Start(proxy);
  EXPECT_FALSE(bridge.has_value());
}

}  // namespace
}  // namespace clawbrowser
