#include "base/command_line.h"
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
