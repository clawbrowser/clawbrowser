#include "base/command_line.h"
#include "clawbrowser/cli/args.h"
#include "clawbrowser/verify/proxy_expectation.h"
#include "clawbrowser/verify/verify_page.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

TEST(VerifyPageTest, FailureExitDisabledWithoutAutomationSwitch) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  EXPECT_FALSE(VerifyFailureExitEnabledForCommandLine(command_line));
}

TEST(VerifyPageTest, FailureExitEnabledWithAutomationSwitch) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch("verify-automation");
  EXPECT_TRUE(VerifyFailureExitEnabledForCommandLine(command_line));
}

TEST(VerifyPageTest, ManagedProxyCapabilityRequiresCompleteLaunchContract) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  EXPECT_EQ(ManagedProxyPrivacyCapabilityForCommandLine(command_line, true),
            0);

  command_line.AppendSwitch(kRequireProxySwitch);
  EXPECT_EQ(ManagedProxyPrivacyCapabilityForCommandLine(command_line, true),
            0);

  command_line.AppendSwitchASCII("proxy-server", "socks5://127.0.0.1:1080");
  EXPECT_EQ(ManagedProxyPrivacyCapabilityForCommandLine(command_line, false),
            0);
  EXPECT_EQ(ManagedProxyPrivacyCapabilityForCommandLine(command_line, true),
            0);

  command_line.AppendSwitchASCII("webrtc-ip-handling-policy",
                                 "disable_non_proxied_udp");
  EXPECT_EQ(ManagedProxyPrivacyCapabilityForCommandLine(command_line, true),
            0);

  command_line.AppendSwitchASCII("force-webrtc-ip-handling-policy",
                                 "disable_non_proxied_udp");
  EXPECT_EQ(ManagedProxyPrivacyCapabilityForCommandLine(command_line, true),
            2);
}

TEST(VerifyPageTest, ManagedProxyCapabilityRejectsWrongWebRtcPolicy) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(kRequireProxySwitch);
  command_line.AppendSwitchASCII("proxy-server", "socks5://127.0.0.1:1080");
  command_line.AppendSwitchASCII("webrtc-ip-handling-policy", "default");
  command_line.AppendSwitchASCII("force-webrtc-ip-handling-policy",
                                 "disable_non_proxied_udp");
  EXPECT_EQ(ManagedProxyPrivacyCapabilityForCommandLine(command_line, true),
            0);
}

TEST(VerifyPageTest, ManagedProxyCapabilityRejectsConflictingProxySwitches) {
  constexpr const char* kConflictingSwitches[] = {
      "no-proxy-server", "proxy-pac-url", "proxy-auto-detect",
      "proxy-bypass-list"};

  for (const char* conflicting_switch : kConflictingSwitches) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch(kRequireProxySwitch);
    command_line.AppendSwitchASCII("proxy-server",
                                   "socks5://127.0.0.1:1080");
    command_line.AppendSwitchASCII("webrtc-ip-handling-policy",
                                   "disable_non_proxied_udp");
    command_line.AppendSwitchASCII("force-webrtc-ip-handling-policy",
                                   "disable_non_proxied_udp");
    command_line.AppendSwitch(conflicting_switch);
    EXPECT_EQ(ManagedProxyPrivacyCapabilityForCommandLine(command_line, true),
              0)
        << conflicting_switch;
  }
}

TEST(VerifyPageTest, ProxyExpectationKeepsCountryOnlyRequestsCountryOnly) {
  ProxyExpectation expected =
      ResolveProxyExpectation("US", std::nullopt, std::string("US"));

  ASSERT_TRUE(expected.country.has_value());
  EXPECT_EQ(*expected.country, "US");
  EXPECT_FALSE(expected.city.has_value());
}

TEST(VerifyPageTest, ProxyExpectationKeepsRequestedCityWhenPresent) {
  ProxyExpectation expected = ResolveProxyExpectation(
      "US", std::make_optional<std::string>("Las Vegas"), std::string("US"));

  ASSERT_TRUE(expected.country.has_value());
  EXPECT_EQ(*expected.country, "US");
  ASSERT_TRUE(expected.city.has_value());
  EXPECT_EQ(*expected.city, "Las Vegas");
}

TEST(VerifyPageTest, ProxyExpectationFallsBackToGeneratedCountryForCityOnly) {
  ProxyExpectation expected = ResolveProxyExpectation(
      std::string(), std::make_optional<std::string>("Las Vegas"),
      std::string("US"));

  ASSERT_TRUE(expected.country.has_value());
  EXPECT_EQ(*expected.country, "US");
  ASSERT_TRUE(expected.city.has_value());
  EXPECT_EQ(*expected.city, "Las Vegas");
}

}  // namespace
}  // namespace clawbrowser
