#include "base/command_line.h"
#include "clawbrowser/cli/args.h"
#include "clawbrowser/generated/fingerprint_types.h"
#include "clawbrowser/verify/proxy_expectation.h"
#include "clawbrowser/verify/verify_page.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

TEST(VerifyPageTest, PrivateProxyWithoutGeoCannotFallBackToRemoteLookup) {
  for (const char* host : {"127.0.0.1", "localhost", "LOCALHOST.",
                           "bridge.localhost", "10.0.0.1", "[::1]"}) {
    EXPECT_TRUE(ProxyHostRequiresClientGeography(host)) << host;
  }
  EXPECT_FALSE(ProxyHostRequiresClientGeography("8.8.8.8"));
  EXPECT_FALSE(ProxyHostRequiresClientGeography("proxy.example.test"));
}

TEST(VerifyPageTest, ClientProxyRequiresMatchingPublicExitCountryAndTimezone) {
  ClientProxyGeo expected;
  expected.ip = "8.8.8.8";
  expected.country = "US";
  expected.timezone = "America/Los_Angeles";
  base::DictValue observed;
  observed.Set("ip", expected.ip);
  observed.Set("country", expected.country);
  observed.Set("timezone", expected.timezone);
  auto result = ClientProxyGeoVerificationResult(expected, observed);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->FindBool("match"), true);
  auto aliased = observed.Clone();
  expected.timezone = "America/Indiana/Indianapolis";
  aliased.Set("timezone", "America/Indianapolis");
  auto alias_result = ClientProxyGeoVerificationResult(expected, aliased);
  ASSERT_TRUE(alias_result.has_value());
  EXPECT_EQ(alias_result->FindBool("match"), true);
  expected.timezone = "America/Los_Angeles";
  for (const char* field : {"ip", "country", "timezone"}) {
    auto changed = observed.Clone();
    changed.Set(field, std::string(field) == "ip" ? "1.1.1.1" :
                       std::string(field) == "country" ? "DE" : "Europe/Berlin");
    auto mismatch = ClientProxyGeoVerificationResult(expected, changed);
    ASSERT_TRUE(mismatch.has_value());
    EXPECT_EQ(mismatch->FindBool("match"), false);
  }
  observed.Set("ip", "127.0.0.1");
  EXPECT_FALSE(ClientProxyGeoVerificationResult(expected, observed).has_value());
  observed.Remove("timezone");
  EXPECT_FALSE(ClientProxyGeoVerificationResult(expected, observed).has_value());
}

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
            0);
  command_line.AppendSwitchASCII("proxy-bypass-list", "<-loopback>");
  EXPECT_EQ(ManagedProxyPrivacyCapabilityForCommandLine(command_line, true),
            2);
}

TEST(VerifyPageTest, ManagedProxyCapabilityRejectsWrongWebRtcPolicy) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(kRequireProxySwitch);
  command_line.AppendSwitchASCII("proxy-server", "socks5://127.0.0.1:1080");
  command_line.AppendSwitchASCII("proxy-bypass-list", "<-loopback>");
  command_line.AppendSwitchASCII("webrtc-ip-handling-policy", "default");
  command_line.AppendSwitchASCII("force-webrtc-ip-handling-policy",
                                 "disable_non_proxied_udp");
  EXPECT_EQ(ManagedProxyPrivacyCapabilityForCommandLine(command_line, true),
            0);
}

TEST(VerifyPageTest, ManagedProxyCapabilityRejectsConflictingProxySwitches) {
  constexpr const char* kConflictingSwitches[] = {
      "no-proxy-server", "proxy-pac-url", "proxy-auto-detect"};

  for (const char* conflicting_switch : kConflictingSwitches) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch(kRequireProxySwitch);
    command_line.AppendSwitchASCII("proxy-server",
                                   "socks5://127.0.0.1:1080");
    command_line.AppendSwitchASCII("webrtc-ip-handling-policy",
                                   "disable_non_proxied_udp");
    command_line.AppendSwitchASCII("force-webrtc-ip-handling-policy",
                                   "disable_non_proxied_udp");
    command_line.AppendSwitchASCII("proxy-bypass-list", "<-loopback>");
    command_line.AppendSwitch(conflicting_switch);
    EXPECT_EQ(ManagedProxyPrivacyCapabilityForCommandLine(command_line, true),
              0)
        << conflicting_switch;
  }
}

TEST(VerifyPageTest, ManagedProxyCapabilityRejectsUnsafeBypassLists) {
  for (const char* bypass : {"", "*", "<local>", "localhost",
                              "<-loopback>;localhost", "<-loopback>,*"}) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch(kRequireProxySwitch);
    command_line.AppendSwitchASCII("proxy-server", "http://127.0.0.1:1080");
    command_line.AppendSwitchASCII("webrtc-ip-handling-policy",
                                   "disable_non_proxied_udp");
    command_line.AppendSwitchASCII("force-webrtc-ip-handling-policy",
                                   "disable_non_proxied_udp");
    command_line.AppendSwitchASCII("proxy-bypass-list", bypass);
    EXPECT_EQ(ManagedProxyPrivacyCapabilityForCommandLine(command_line, true), 0)
        << bypass;
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
