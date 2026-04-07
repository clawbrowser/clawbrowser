#include "base/command_line.h"
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

}  // namespace
}  // namespace clawbrowser
