#include "clawbrowser/cli/args.h"

#include "base/command_line.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace clawbrowser {
namespace {

TEST(ArgsTest, ParseFingerprintId) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "fp_abc123");

  ClawArgs args = ClawArgs::Parse(cmd);
  EXPECT_TRUE(args.has_fingerprint());
  EXPECT_EQ(args.fingerprint_id(), "fp_abc123");
  EXPECT_FALSE(args.regenerate());
  EXPECT_FALSE(args.list());
  EXPECT_FALSE(args.verbose());
  EXPECT_FALSE(args.skip_verify());
}

TEST(ArgsTest, ParseRegenerate) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "fp_abc123");
  cmd.AppendSwitch("regenerate");

  ClawArgs args = ClawArgs::Parse(cmd);
  EXPECT_TRUE(args.has_fingerprint());
  EXPECT_TRUE(args.regenerate());
}

TEST(ArgsTest, ParseList) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitch("list");

  ClawArgs args = ClawArgs::Parse(cmd);
  EXPECT_TRUE(args.list());
  EXPECT_FALSE(args.has_fingerprint());
}

TEST(ArgsTest, ParseVerbose) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitch("verbose");

  ClawArgs args = ClawArgs::Parse(cmd);
  EXPECT_TRUE(args.verbose());
}

TEST(ArgsTest, ParseOutputJson) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("output", "json");

  ClawArgs args = ClawArgs::Parse(cmd);
  EXPECT_TRUE(args.json_output());
}

TEST(ArgsTest, ParseSkipVerify) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitch("skip-verify");

  ClawArgs args = ClawArgs::Parse(cmd);
  EXPECT_TRUE(args.skip_verify());
}

TEST(ArgsTest, ParseVerifyAutomation) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitch("verify-automation");

  ClawArgs args = ClawArgs::Parse(cmd);
  EXPECT_TRUE(args.verify_automation());
}

TEST(ArgsTest, ParseLocationOverrides) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("country", "DE");
  cmd.AppendSwitchASCII("city", "Berlin");
  cmd.AppendSwitchASCII("connection-type", "mobile");

  ClawArgs args = ClawArgs::Parse(cmd);
  EXPECT_TRUE(args.has_country_override());
  EXPECT_TRUE(args.has_city_override());
  EXPECT_TRUE(args.has_connection_type_override());
  EXPECT_TRUE(args.has_location_overrides());
  EXPECT_EQ(args.country(), "DE");
  ASSERT_TRUE(args.city().has_value());
  EXPECT_EQ(*args.city(), "Berlin");
  ASSERT_TRUE(args.connection_type().has_value());
  EXPECT_EQ(*args.connection_type(), "mobile");
}

TEST(ArgsTest, ParseCityOnlyOverride) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("city", "Berlin");

  ClawArgs args = ClawArgs::Parse(cmd);
  EXPECT_FALSE(args.has_country_override());
  EXPECT_TRUE(args.has_city_override());
  EXPECT_FALSE(args.has_connection_type_override());
  EXPECT_TRUE(args.has_location_overrides());
  EXPECT_TRUE(args.country().empty());
  ASSERT_TRUE(args.city().has_value());
  EXPECT_EQ(*args.city(), "Berlin");
}

TEST(ArgsTest, VanillaMode) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);

  ClawArgs args = ClawArgs::Parse(cmd);
  EXPECT_FALSE(args.has_fingerprint());
  EXPECT_TRUE(args.is_vanilla());
}

TEST(ArgsTest, InvalidFingerprintId) {
  base::CommandLine cmd(base::CommandLine::NO_PROGRAM);
  cmd.AppendSwitchASCII("fingerprint", "");

  ClawArgs args = ClawArgs::Parse(cmd);
  EXPECT_TRUE(args.has_fingerprint());
  EXPECT_TRUE(args.fingerprint_id().empty());
  // Validation happens in profile_manager, not args
}

}  // namespace
}  // namespace clawbrowser
