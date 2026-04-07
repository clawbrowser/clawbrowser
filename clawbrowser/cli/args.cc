#include "clawbrowser/cli/args.h"

namespace clawbrowser {

ClawArgs::ClawArgs()
    : has_fingerprint_(false),
      regenerate_(false),
      list_(false),
      verbose_(false),
      json_output_(false),
      skip_verify_(false),
      verify_automation_(false) {}

ClawArgs::ClawArgs(const ClawArgs&) = default;

ClawArgs& ClawArgs::operator=(const ClawArgs&) = default;

ClawArgs::ClawArgs(ClawArgs&&) = default;

ClawArgs& ClawArgs::operator=(ClawArgs&&) = default;

ClawArgs::~ClawArgs() = default;

// static
ClawArgs ClawArgs::Parse(const base::CommandLine& command_line) {
  ClawArgs args;

  if (command_line.HasSwitch("fingerprint")) {
    args.has_fingerprint_ = true;
    args.fingerprint_id_ = command_line.GetSwitchValueASCII("fingerprint");
  }

  args.regenerate_ = command_line.HasSwitch("regenerate");
  args.list_ = command_line.HasSwitch("list");
  args.verbose_ = command_line.HasSwitch("verbose");
  args.skip_verify_ = command_line.HasSwitch("skip-verify");
  args.verify_automation_ = command_line.HasSwitch("verify-automation");

  if (command_line.HasSwitch("output")) {
    args.json_output_ =
        command_line.GetSwitchValueASCII("output") == "json";
  }

  return args;
}

}  // namespace clawbrowser
