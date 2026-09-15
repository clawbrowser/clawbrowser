#include "clawbrowser/proxy/proxy_config.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

TEST(ProxyConfigTest, BuildFromFingerprint) {
  RuntimeProxyConfig proxy;
  proxy.scheme = "http";
  proxy.host = "proxy.nodemaven.com";
  proxy.port = 8080;
  proxy.username = "user_abc";
  proxy.password = "pass_xyz";

  auto result = BuildClawbrowserProxyConfig(proxy);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(result->proxy_server, "http://proxy.nodemaven.com:8080");
  EXPECT_EQ(result->username, "user_abc");
  EXPECT_EQ(result->password, "pass_xyz");
}

TEST(ProxyConfigTest, BuildReturnsNullForNoProxy) {
  auto result = BuildClawbrowserProxyConfig(std::nullopt);
  EXPECT_FALSE(result.has_value());
}

TEST(ProxyConfigTest, BuildReturnsNullForMissingEndpoint) {
  RuntimeProxyConfig proxy;
  proxy.username = "user";
  proxy.password = "pass";

  auto result = BuildClawbrowserProxyConfig(proxy);
  EXPECT_FALSE(result.has_value());
}

TEST(ProxyConfigTest, GeneratesDefaultHttpProxyServerFlag) {
  RuntimeProxyConfig proxy;
  proxy.host = "proxy.example.com";
  proxy.port = 3128;
  proxy.username = "user";
  proxy.password = "pass";

  auto flags = GetProxyCommandLineFlags(proxy);
  ASSERT_EQ(flags.size(), 4u);
  EXPECT_EQ(flags[3], "--proxy-bypass-list=<-loopback>");
  EXPECT_EQ(flags[0], "--proxy-server=http://proxy.example.com:3128");
  EXPECT_EQ(flags[1],
            "--webrtc-ip-handling-policy=disable_non_proxied_udp");
  EXPECT_EQ(flags[2],
            "--force-webrtc-ip-handling-policy=disable_non_proxied_udp");
}

TEST(ProxyConfigTest, GeneratesHttpProxyServerFlag) {
  RuntimeProxyConfig proxy;
  proxy.scheme = "http";
  proxy.host = "proxy.example.com";
  proxy.port = 3128;

  auto flags = GetProxyCommandLineFlags(proxy);
  ASSERT_EQ(flags.size(), 4u);
  EXPECT_EQ(flags[3], "--proxy-bypass-list=<-loopback>");
  EXPECT_EQ(flags[0], "--proxy-server=http://proxy.example.com:3128");
  EXPECT_EQ(flags[1],
            "--webrtc-ip-handling-policy=disable_non_proxied_udp");
  EXPECT_EQ(flags[2],
            "--force-webrtc-ip-handling-policy=disable_non_proxied_udp");
}

TEST(ProxyConfigTest, GeneratesSocks5ProxyServerFlag) {
  RuntimeProxyConfig proxy;
  proxy.scheme = "socks5";
  proxy.host = "proxy.example.com";
  proxy.port = 1080;

  auto flags = GetProxyCommandLineFlags(proxy);
  ASSERT_EQ(flags.size(), 4u);
  EXPECT_EQ(flags[3], "--proxy-bypass-list=<-loopback>");
  EXPECT_EQ(flags[0], "--proxy-server=socks5://proxy.example.com:1080");
  EXPECT_EQ(flags[1],
            "--webrtc-ip-handling-policy=disable_non_proxied_udp");
  EXPECT_EQ(flags[2],
            "--force-webrtc-ip-handling-policy=disable_non_proxied_udp");
}

TEST(ProxyConfigTest, AuthenticatedSocks5UsesHttpBridgeEndpoint) {
  RuntimeProxyConfig proxy;
  proxy.scheme = "socks5";
  proxy.host = "gate.nodemaven.com";
  proxy.port = 1080;
  proxy.username = "user_abc";
  proxy.password = "pass_xyz";

  EXPECT_TRUE(ShouldUseSocks5AuthBridge(proxy));

  auto flags = GetProxyCommandLineFlags(
      proxy, ProxyBridgeEndpoint{.host = "127.0.0.1", .port = 43210});
  ASSERT_EQ(flags.size(), 4u);
  EXPECT_EQ(flags[3], "--proxy-bypass-list=<-loopback>");
  EXPECT_EQ(flags[0], "--proxy-server=http://127.0.0.1:43210");
  EXPECT_EQ(flags[1],
            "--webrtc-ip-handling-policy=disable_non_proxied_udp");
  EXPECT_EQ(flags[2],
            "--force-webrtc-ip-handling-policy=disable_non_proxied_udp");
  EXPECT_EQ(flags[0].find("user_abc"), std::string::npos);
  EXPECT_EQ(flags[0].find("pass_xyz"), std::string::npos);
}

TEST(ProxyConfigTest, UnauthenticatedSocks5KeepsNativeSocksRoute) {
  RuntimeProxyConfig proxy;
  proxy.scheme = "socks5";
  proxy.host = "proxy.example.com";
  proxy.port = 1080;

  EXPECT_FALSE(ShouldUseSocks5AuthBridge(proxy));

  auto flags = GetProxyCommandLineFlags(
      proxy, ProxyBridgeEndpoint{.host = "127.0.0.1", .port = 43210});
  ASSERT_EQ(flags.size(), 4u);
  EXPECT_EQ(flags[3], "--proxy-bypass-list=<-loopback>");
  EXPECT_EQ(flags[0], "--proxy-server=socks5://proxy.example.com:1080");
  EXPECT_EQ(flags[1],
            "--webrtc-ip-handling-policy=disable_non_proxied_udp");
  EXPECT_EQ(flags[2],
            "--force-webrtc-ip-handling-policy=disable_non_proxied_udp");
}

TEST(ProxyConfigTest, IncompleteSocks5CredentialsDoNotUseBridge) {
  RuntimeProxyConfig proxy;
  proxy.scheme = "socks5";
  proxy.host = "proxy.example.com";
  proxy.port = 1080;
  proxy.username = "user_only";

  EXPECT_FALSE(ShouldUseSocks5AuthBridge(proxy));

  auto flags = GetProxyCommandLineFlags(
      proxy, ProxyBridgeEndpoint{.host = "127.0.0.1", .port = 43210});
  ASSERT_EQ(flags.size(), 4u);
  EXPECT_EQ(flags[3], "--proxy-bypass-list=<-loopback>");
  EXPECT_EQ(flags[0], "--proxy-server=socks5://proxy.example.com:1080");
  EXPECT_EQ(flags[1],
            "--webrtc-ip-handling-policy=disable_non_proxied_udp");
  EXPECT_EQ(flags[2],
            "--force-webrtc-ip-handling-policy=disable_non_proxied_udp");
}

TEST(ProxyConfigTest, InvalidSchemeProducesNoProxyFlags) {
  RuntimeProxyConfig proxy;
  proxy.scheme = "socks4";
  proxy.host = "proxy.example.com";
  proxy.port = 1080;

  EXPECT_FALSE(BuildClawbrowserProxyConfig(proxy).has_value());
  EXPECT_TRUE(GetProxyCommandLineFlags(proxy).empty());
  EXPECT_FALSE(ShouldUseSocks5AuthBridge(proxy));
}

TEST(ProxyConfigTest, ParsesDevProxyUrl) {
  auto proxy =
      ParseProxyUrl("socks5://user_abc:pass_xyz@gate.nodemaven.com:1080");
  ASSERT_TRUE(proxy.has_value()) << proxy.error();

  EXPECT_EQ(proxy->scheme.value_or(""), "socks5");
  EXPECT_EQ(proxy->host.value_or(""), "gate.nodemaven.com");
  EXPECT_EQ(proxy->port.value_or(0), 1080);
  EXPECT_EQ(proxy->username.value_or(""), "user_abc");
  EXPECT_EQ(proxy->password.value_or(""), "pass_xyz");
}

TEST(ProxyConfigTest, RejectsInvalidProxyUrlScheme) {
  auto proxy =
      ParseProxyUrl("socks4://user_abc:pass_xyz@gate.nodemaven.com:1080");
  ASSERT_FALSE(proxy.has_value());
  EXPECT_NE(proxy.error().find("unsupported proxy scheme"), std::string::npos);
}

TEST(ProxyConfigTest, RejectsProxyUrlWithIncompleteCredentials) {
  auto proxy = ParseProxyUrl("socks5://user_abc@gate.nodemaven.com:1080");
  ASSERT_FALSE(proxy.has_value());
  EXPECT_NE(proxy.error().find("username and password"), std::string::npos);
}

TEST(ProxyConfigTest, SkipsProxyServerFlagWhenEndpointIncomplete) {
  RuntimeProxyConfig proxy;
  proxy.username = "user";
  proxy.password = "pass";

  auto flags = GetProxyCommandLineFlags(proxy);
  EXPECT_TRUE(flags.empty());
}

}  // namespace
}  // namespace clawbrowser
