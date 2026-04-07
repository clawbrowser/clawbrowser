#include "clawbrowser/proxy/proxy_config.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

TEST(ProxyConfigTest, BuildFromFingerprint) {
  RuntimeProxyConfig proxy;
  proxy.scheme = "https";
  proxy.host = "proxy.nodemaven.com";
  proxy.port = 8080;
  proxy.username = "user_abc";
  proxy.password = "pass_xyz";

  auto result = BuildChromiumProxyConfig(proxy);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(result->proxy_server, "https://proxy.nodemaven.com:8080");
  EXPECT_EQ(result->username, "user_abc");
  EXPECT_EQ(result->password, "pass_xyz");
}

TEST(ProxyConfigTest, BuildReturnsNullForNoProxy) {
  auto result = BuildChromiumProxyConfig(std::nullopt);
  EXPECT_FALSE(result.has_value());
}

TEST(ProxyConfigTest, BuildReturnsNullForMissingEndpoint) {
  RuntimeProxyConfig proxy;
  proxy.username = "user";
  proxy.password = "pass";

  auto result = BuildChromiumProxyConfig(proxy);
  EXPECT_FALSE(result.has_value());
}

TEST(ProxyConfigTest, GeneratesProxyServerFlag) {
  RuntimeProxyConfig proxy;
  proxy.scheme = "https";
  proxy.host = "proxy.example.com";
  proxy.port = 3128;
  proxy.username = "user";
  proxy.password = "pass";

  auto flags = GetProxyCommandLineFlags(proxy);
  EXPECT_EQ(flags.size(), 1u);
  EXPECT_EQ(flags[0], "--proxy-server=https://proxy.example.com:3128");
}

TEST(ProxyConfigTest, GeneratesHttpProxyServerFlag) {
  RuntimeProxyConfig proxy;
  proxy.scheme = "http";
  proxy.host = "proxy.example.com";
  proxy.port = 3128;

  auto flags = GetProxyCommandLineFlags(proxy);
  EXPECT_EQ(flags.size(), 1u);
  EXPECT_EQ(flags[0], "--proxy-server=http://proxy.example.com:3128");
}

TEST(ProxyConfigTest, GeneratesSocks5ProxyServerFlag) {
  RuntimeProxyConfig proxy;
  proxy.scheme = "socks5";
  proxy.host = "proxy.example.com";
  proxy.port = 1080;

  auto flags = GetProxyCommandLineFlags(proxy);
  EXPECT_EQ(flags.size(), 1u);
  EXPECT_EQ(flags[0], "--proxy-server=socks5://proxy.example.com:1080");
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
