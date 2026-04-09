#include <optional>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "clawbrowser/fingerprint_accessor.h"
#include "clawbrowser/proxy/proxy_auth_login_delegate.h"
#include "clawbrowser/proxy/proxy_auth_preloader.h"
#include "net/base/auth.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

using CallbackResult = std::optional<std::optional<net::AuthCredentials>>;

class ProxyAuthLoginDelegateTest : public testing::Test {
 protected:
  void SetUp() override {
    FingerprintAccessor::Reset();
  }

  void TearDown() override {
    FingerprintAccessor::Reset();
  }

  void InstallProxyCredentials(const std::string& username,
                               const std::string& password) {
    RuntimeProxyConfig proxy;
    proxy.host = "proxy.example.com";
    proxy.port = 3128;
    proxy.username = username;
    proxy.password = password;
    FingerprintAccessor::Set(RuntimeFingerprint(), proxy);
  }

  net::AuthChallengeInfo MakeProxyChallenge() {
    net::AuthChallengeInfo auth_info;
    auth_info.is_proxy = true;
    return auth_info;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(ProxyAuthLoginDelegateTest, ResolvesCredentialsOnNextTaskTurn) {
  InstallProxyCredentials("user_abc", "pass_xyz");

  CallbackResult callback_result;
  ProxyAuthLoginDelegate delegate(
      MakeProxyChallenge(), nullptr,
      base::BindOnce(
          [](CallbackResult* out,
             const std::optional<net::AuthCredentials>& credentials) {
            *out = std::move(credentials);
          },
          &callback_result));

  EXPECT_FALSE(callback_result.has_value());

  task_environment_.RunUntilIdle();

  ASSERT_TRUE(callback_result.has_value());
  ASSERT_TRUE((*callback_result).has_value());
  EXPECT_EQ((*callback_result)->username(), u"user_abc");
  EXPECT_EQ((*callback_result)->password(), u"pass_xyz");
}

TEST_F(ProxyAuthLoginDelegateTest, ResolvesNulloptOnNextTaskTurnWithoutProxy) {
  CallbackResult callback_result;
  ProxyAuthLoginDelegate delegate(
      MakeProxyChallenge(), nullptr,
      base::BindOnce(
          [](CallbackResult* out,
             const std::optional<net::AuthCredentials>& credentials) {
            *out = std::move(credentials);
          },
          &callback_result));

  EXPECT_FALSE(callback_result.has_value());

  task_environment_.RunUntilIdle();

  ASSERT_TRUE(callback_result.has_value());
  EXPECT_FALSE((*callback_result).has_value());
}

TEST(ProxyAuthPreloaderTest, BuildsHttpsBasicProxyChallenge) {
  RuntimeProxyConfig proxy;
  proxy.scheme = "https";
  proxy.host = "proxy.example.com";
  proxy.port = 3128;
  proxy.username = "user_abc";
  proxy.password = "pass_xyz";

  std::optional<ProxyAuthPreloadConfig> config =
      BuildProxyAuthPreloadConfig(proxy);

  ASSERT_TRUE(config.has_value());
  EXPECT_TRUE(config->challenge.is_proxy);
  EXPECT_EQ(config->challenge.challenger.scheme(), "https");
  EXPECT_EQ(config->challenge.challenger.host(), "proxy.example.com");
  EXPECT_EQ(config->challenge.challenger.port(), 3128);
  EXPECT_EQ(config->challenge.scheme, "basic");
  EXPECT_EQ(config->challenge.realm, "clawbrowser");
  EXPECT_EQ(config->challenge.challenge, "Basic realm=\"clawbrowser\"");
  EXPECT_EQ(config->challenge.path, "/");
  EXPECT_EQ(config->credentials.username(), u"user_abc");
  EXPECT_EQ(config->credentials.password(), u"pass_xyz");
}

TEST(ProxyAuthPreloaderTest, SkipsSocks5ProxyChallengePreload) {
  RuntimeProxyConfig proxy;
  proxy.scheme = "socks5";
  proxy.host = "proxy.example.com";
  proxy.port = 1080;
  proxy.username = "user_abc";
  proxy.password = "pass_xyz";

  EXPECT_FALSE(BuildProxyAuthPreloadConfig(proxy).has_value());
}

TEST(ProxyAuthPreloaderTest, SkipsMissingProxyCredentials) {
  RuntimeProxyConfig proxy;
  proxy.host = "proxy.example.com";
  proxy.port = 3128;
  proxy.username = "user_abc";

  EXPECT_FALSE(BuildProxyAuthPreloadConfig(proxy).has_value());
}

}  // namespace
}  // namespace clawbrowser
